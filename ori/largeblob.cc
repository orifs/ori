/*
 * Copyright (c) 2012 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>

#include <cstdlib>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <errno.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include <openssl/sha.h>

#ifdef OPENSSL_NO_SHA256
#error "SHA256 not supported!"
#endif

#include "util.h"
#include "largeblob.h"
#include "chunker.h"
#include "repo.h"

using namespace std;

/********************************************************************
 *
 *
 * LBlobEntry
 *
 *
 ********************************************************************/

LBlobEntry::LBlobEntry(const LBlobEntry &l)
    : hash(l.hash), length(l.length)
{
}

LBlobEntry::LBlobEntry(const std::string &h, uint16_t l)
    : hash(h), length(l)
{
}

LBlobEntry::~LBlobEntry()
{
}

/********************************************************************
 *
 *
 * LargeBlob
 *
 *
 ********************************************************************/

LargeBlob::LargeBlob(Repo *r)
{
    repo = r;
}

LargeBlob::~LargeBlob()
{
}

class FileChunkerCB : public ChunkerCB
{
public:
    FileChunkerCB(LargeBlob *l)
    {
	lb = l;
	lbOff = 0;
    }
    ~FileChunkerCB()
    {
    }
    int open(const string &path)
    {
	struct stat sb;

        bufLen = 8 * 1024 * 1024;
	buf = new uint8_t[bufLen];
	if (buf == NULL)
	    return -ENOMEM;

	srcFd = ::open(path.c_str(), O_RDONLY);
	if (srcFd < 0)
	    return -errno;

	if (fstat(srcFd, &sb) < 0) {
	    close(srcFd);
	    return -errno;
	}
	fileLen = sb.st_size;
        fileOff = 0;

	return 0;
    }
    virtual void match(const uint8_t *b, uint32_t l)
    {
	SHA256_CTX state;
	unsigned char hash[SHA256_DIGEST_LENGTH];
	stringstream ss;

	SHA256_Init(&state);
	SHA256_Update(&state, b, l);
	SHA256_Final(hash, &state);

	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
	    ss << hex << setw(2) << setfill('0') << (int)hash[i];
	}

        // Add the fragment into the repository
        // XXX: Journal for cleanup!
        string blob = string((const char *)b, l);
        //blob.set(b, l);
        lb->repo->addBlob(blob, Object::Blob);

        // Add the fragment to the LargeBlob object.
	lb->parts.insert(make_pair(lbOff, LBlobEntry(ss.str(), l)));
	lbOff += l;
    }
    virtual int load(uint8_t **b, uint64_t *l, uint64_t *o)
    {
        if (*b == NULL)
            *b = buf;

        if (fileOff == fileLen)
            return 0;

        // Sanity checking
        assert(*b == buf);
        assert(*l <= bufLen);
        assert(*o <= bufLen);

        if (*o != 0 || *o != *l) {
            assert(*o > 32); // XXX: Must equal hashLen
            assert(*l > 32);
            memcpy(buf, buf + *o - 32, *l - *o + 32);
            *l = *l - *o + 32;
            *o = 32;
        }

        uint64_t toRead = MIN(bufLen - *l, fileLen - fileOff);
        int status;

        status = read(srcFd, buf + *l, toRead);
        if (status < 0) {
            perror("Cannot read large file");
            assert(false);
            return -1;
        }
        assert(status == toRead);

        fileOff += status;
        *l += status;
        //*o = 32;

	return 1;
    }
private:
    // Output large blob
    LargeBlob *lb;
    uint64_t lbOff;
    // Input file
    int srcFd;
    uint64_t fileLen;
    uint64_t fileOff;
    // RK buffer
    uint8_t *buf;
    uint64_t bufLen;
};

void
LargeBlob::chunkFile(const string &path)
{
    int status;
    FileChunkerCB cb = FileChunkerCB(this);
    Chunker<4096, 2048, 8192> c = Chunker<4096, 2048, 8192>();

    status = cb.open(path);
    if (status < 0) {
	perror("Cannot open large file for chunking");
	assert(false);
	return;
    }

    hash = Util_HashFile(path);

    c.chunk(&cb);
}

void
LargeBlob::extractFile(const string &path)
{
    int fd;
    map<uint64_t, LBlobEntry>::iterator it;

    fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
	        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        perror("Cannot open file for writing");
        assert(false);
        return;
    }

    for (it = parts.begin(); it != parts.end(); it++)
    {
        int status;
        string tmp;

        tmp = repo->getPayload((*it).second.hash);
        assert(tmp.length() == (*it).second.length);

        status = ::write(fd, tmp.data(), tmp.length());
        if (status < 0) {
            perror("write to large object failed");
            assert(false);
            return;
        }

        assert(status == tmp.length());
    }

#ifdef DEBUG
    string extractedHash = Util_HashFile(path);
    assert(extractedHash == hash);
#endif /* DEBUG */
}

const string
LargeBlob::getBlob()
{
    string blob = hash + "\n";
    map<uint64_t, LBlobEntry>::iterator it;

    for (it = parts.begin(); it != parts.end(); it++)
    {
        stringstream ss;

	blob += (*it).second.hash + " ";
        ss << (*it).second.length;
        blob += ss.str();
	blob += "\n";
    }

    return blob;
}

void
LargeBlob::fromBlob(const string &blob)
{
    uint64_t off = 0;
    string line;
    stringstream ss(blob);

    getline(ss, line, '\n');
    hash = line;

    while (getline(ss, line, '\n')) {
        string hash;
        string length;
        uint64_t len;

	hash = line.substr(0, 64);
        length = line.substr(65);
        len = strtoul(length.c_str(), NULL, 10);

	parts.insert(make_pair(off, LBlobEntry(hash, len)));

        off += len;
    }
}

