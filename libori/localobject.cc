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

#define __STDC_LIMIT_MACROS

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include <openssl/sha.h>

#ifdef OPENSSL_NO_SHA256
#error "SHA256 not supported!"
#endif

#include "debug.h"
#include "tuneables.h"
#include "object.h"
#include "localobject.h"

using namespace std;

/*
 * Object
 */
LocalObject::LocalObject()
    : fd(-1), storedLen(0), fileSize(0)
{
}

LocalObject::~LocalObject()
{
    close();
}

/*
 * Create a new object.
 */
int
LocalObject::create(const string &path, Type type, uint32_t flags)
{
    int status;

    objPath = path;

    fd = ::open(path.c_str(), O_CREAT | O_RDWR,
	        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0)
	return -errno;

    info.type = type;
    info.flags = flags;
    status = info.writeTo(fd);
    if (status < 0)
        return -errno;

    // Stored length
    status = pwrite(fd, "\0\0\0\0\0\0\0\0",
            ORI_OBJECT_SIZE, 16);
    if (status < 0)
        return -errno;
    assert(status == ORI_OBJECT_SIZE);

    fileSize = ORI_OBJECT_HDRSIZE;

    return 0;
}

/*
 * Create a new object from existing raw data
 */
int
LocalObject::createFromRawData(const string &path, const ObjectInfo &info,
        const string &raw_data)
{
    objPath = path;
    fd = ::open(path.c_str(), O_CREAT | O_RDWR,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0)
        return -errno;

    assert(info.hasAllFields());
    this->info = info;
    int status = this->info.writeTo(fd);
    if (status < 0)
        return -errno;

    // Store data
    storedLen = raw_data.length();
    status = pwrite(fd, &storedLen, ORI_OBJECT_SIZE, 16);
    if (status < 0)
        return -errno;

    status = pwrite(fd, raw_data.data(), raw_data.length(), 24);
    if (status < 0)
        return -errno;

    fileSize = storedLen + ORI_OBJECT_HDRSIZE;

    return 0;
}

/*
 * Open an existing object read-only.
 */
int
LocalObject::open(const string &path, const string &hash)
{
    int status;
    char header[24];
    char buf[5];

    objPath = path;

    fd = ::open(path.c_str(), O_RDWR);
    if (fd < 0) {
        perror("open");
        printf("object open error %s\n", path.c_str());
	return -errno;
    }

    status = pread(fd, header, 24, 0);
    if (status < 0) {
        int en = errno;
        perror("pread object header");
	close();
	return -en;
    }

    assert(status == 24);

    memcpy(buf, header, 4);
    buf[4] = '\0';

    info.type = getTypeForStr(buf);
    if (info.type == Null) {
        printf("Unknown object type!\n");
        assert(false);
    }

    memcpy(&info.flags, header + 4, ORI_OBJECT_FLAGSSIZE);
    memcpy(&info.payload_size, header + 8, ORI_OBJECT_SIZE);
    memcpy(&storedLen, header + 16, ORI_OBJECT_SIZE);

    // Read filesize
    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        int en = errno;
        perror("fstat");
        close();
	return -en;
    }
    fileSize = sb.st_size;

    if (hash.size() > 0) {
        assert(hash.size() == 64);
        info.hash = hash;
    }

    return 0;
}

/*
 * Close the object file.
 */
void
LocalObject::close()
{
    if (fd > 0)
        ::close(fd);

    fd = -1;
    storedLen = 0;
    objPath = "";
    info = ObjectInfo();
}

/*
 * Get the on-disk file size including the object header.
 */
size_t
LocalObject::getFileSize()
{
#ifdef DEBUG
    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        perror("fstat");
        return 0;
    }
    assert(fileSize == (size_t)sb.st_size);
#endif

    return fileSize;
}

/*
 * Get the size of the payload when stored on disk (e.g. after compression)
 */
size_t
LocalObject::getStoredPayloadSize() {
    return storedLen;
}

/*
 * Purge object.
 */
int
LocalObject::purge()
{
    int status;

    // XXX: Support for backrefs and large blobs
    NOT_IMPLEMENTED(ORI_OBJECT_HDRSIZE + storedLen == getFileSize());
    NOT_IMPLEMENTED(info.type == Blob);

    status = pwrite(fd, "PURG", ORI_OBJECT_TYPESIZE, 0);
    info.type = Purged;
    if (status < 0)
	return -errno;

    status = ftruncate(fd, ORI_OBJECT_HDRSIZE);
    if (status < 0)
	return -errno;

    fileSize = ORI_OBJECT_HDRSIZE;

    return 0;
}

/*
 * Append the specified file into the object.
 */
int
LocalObject::setPayload(bytestream *bs)
{
    //std::auto_ptr<bytestream> bs(bs_in);
    if (bs == NULL)
        return -1;
    if (bs->error()) {
        puts(bs->error());
        return -bs->errnum();
    }

    lzma_stream strm = LZMA_STREAM_INIT;
    if (info.getCompressed()) setupLzma(&strm, true);

    if (lseek(fd, ORI_OBJECT_HDRSIZE, SEEK_SET) != ORI_OBJECT_HDRSIZE) {
        return -errno;
    }

    size_t totalRead = 0;
    uint8_t buf[COPYFILE_BUFSZ];
    while (!bs->ended()) {
        size_t bytesRead = bs->read(buf, COPYFILE_BUFSZ);
        if (bs->error())
            return -bs->errnum();
        totalRead += bytesRead;
        
        if (info.getCompressed()) {
            strm.next_in = buf;
            strm.avail_in = bytesRead;
            appendLzma(fd, &strm, LZMA_RUN);
        }
        else {
retryWrite:
            ssize_t bytesWritten = write(fd, buf, bytesRead);
            if (bytesWritten < 0) {
                if (errno == EINTR)
                    goto retryWrite;
                perror("write");
                return -errno;
            }

            assert(bytesRead == (size_t)bytesWritten);
        }
    }

    info.payload_size = totalRead;
    if (info.writeTo(fd) < 0) {
        return -errno;
    }

    // Write final (post-compression) size
    if (info.getCompressed()) {
        appendLzma(fd, &strm, LZMA_FINISH);
        storedLen = strm.total_out;
        lzma_end(&strm);
    }
    else {
        storedLen = info.payload_size;
    }

    if (pwrite(fd, (void *)&storedLen, ORI_OBJECT_SIZE, 16) != ORI_OBJECT_SIZE)
        return -errno;

    fileSize = ORI_OBJECT_HDRSIZE + storedLen; // TODO

    return storedLen;
}

/*
 * Append a blob to the object file.
 */
int
LocalObject::setPayload(const string &blob)
{
    int status;

    info.payload_size = blob.length();
    if (info.writeTo(fd) < 0) {
        return -errno;
    }

    if (info.getCompressed()) {
        if (lseek(fd, ORI_OBJECT_HDRSIZE, SEEK_SET) != ORI_OBJECT_HDRSIZE)
            return -errno;

        lzma_stream strm = LZMA_STREAM_INIT;
        setupLzma(&strm, true);

        strm.next_in = (const uint8_t *)blob.data();
        strm.avail_in = blob.length();
        appendLzma(fd, &strm, LZMA_RUN);
        appendLzma(fd, &strm, LZMA_FINISH);

        storedLen = strm.total_out;
        lzma_end(&strm);
    }
    else {
        status = pwrite(fd, blob.data(), blob.length(), ORI_OBJECT_HDRSIZE);
        if (status < 0)
            return -errno;

        assert((size_t)status == blob.length());

        storedLen = blob.length();
    }

    status = pwrite(fd, (void *)&storedLen, ORI_OBJECT_SIZE, 16);
    if (status < 0)
        return -errno;

    fileSize = ORI_OBJECT_HDRSIZE + storedLen; // TODO

    return 0;
}

/*
 * Recompute the SHA-256 hash to verify the file.
 */
string
LocalObject::computeHash()
{
    std::auto_ptr<bytestream> bs(getPayloadStream());
    if (bs->error()) return "";

    uint8_t buf[COPYFILE_BUFSZ];
    unsigned char hash[SHA256_DIGEST_LENGTH];
    stringstream rval;

    SHA256_CTX state;
    SHA256_Init(&state);

    while(!bs->ended()) {
        size_t bytesRead = bs->read(buf, COPYFILE_BUFSZ);
        if (bs->error()) {
            return "";
        }

        SHA256_Update(&state, buf, bytesRead);
    }

    SHA256_Final(hash, &state);

    // Convert into string.
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
	rval << hex << setw(2) << setfill('0') << (int)hash[i];
    }

    return rval.str();
}

bytestream::ap LocalObject::getPayloadStream() {
    if (info.getCompressed()) {
        bytestream::ap bs(getStoredPayloadStream());
        return bytestream::ap(new lzmastream(bs.release(), false, info.payload_size));
    }
    else {
        return getStoredPayloadStream();
    }
}

bytestream::ap LocalObject::getStoredPayloadStream() {
    return bytestream::ap(new fdstream(fd, ORI_OBJECT_HDRSIZE, storedLen));
}


/*
 * Static methods
 */



/*
 * Private methods
 */

// Code from xz_pipe_comp.c in xz examples

void LocalObject::setupLzma(lzma_stream *strm, bool encode) {
    lzma_ret ret_xz;
    if (encode) {
        ret_xz = lzma_easy_encoder(strm, 0, LZMA_CHECK_NONE);
    }
    else {
        ret_xz = lzma_stream_decoder(strm, UINT64_MAX, 0);
    }
    assert(ret_xz == LZMA_OK);
}

bool LocalObject::appendLzma(int dstFd, lzma_stream *strm, lzma_action action) {
    uint8_t outbuf[COPYFILE_BUFSZ];
    do {
        strm->next_out = outbuf;
        strm->avail_out = COPYFILE_BUFSZ;

        lzma_ret ret_xz = lzma_code(strm, action);
        if (ret_xz == LZMA_OK || ret_xz == LZMA_STREAM_END) {
            size_t bytes_to_write = COPYFILE_BUFSZ - strm->avail_out;
            int err = write(dstFd, outbuf, bytes_to_write);
            if (err < 0) {
                // TODO: retry write if interrupted?
                return false;
            }
        }
        else {
            ori_log("lzma_code error: %d\n", (int)ret_xz);
            return false;
        }
    } while (strm->avail_out == 0); // i.e. output isn't finished

    return true;
}

