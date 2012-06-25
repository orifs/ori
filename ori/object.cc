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

/*
 * TODO:
 *  - Object Compression
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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
#include "object.h"

using namespace std;

Object::Object()
{
    fd = -1;
    flags = 0;
}

Object::~Object()
{
    close();
}

/*
 * Create a new object.
 */
int
Object::create(const string &path, Type type, uint32_t flags)
{
    int status;

    objPath = path;

    fd = ::open(path.c_str(), O_CREAT | O_RDWR,
	        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0)
	return -errno;

    switch (type) {
	case Commit:
	    status = pwrite(fd, "CMMT", ORI_OBJECT_TYPESIZE, 0);
	    t = Commit;
	    break;
	case Tree:
	    status = pwrite(fd, "TREE", ORI_OBJECT_TYPESIZE, 0);
	    t = Tree;
	    break;
	case Blob:
	    status = pwrite(fd, "BLOB", ORI_OBJECT_TYPESIZE, 0);
	    t = Blob;
	    break;
    case LargeBlob:
	    status = pwrite(fd, "LGBL", ORI_OBJECT_TYPESIZE, 0);
	    t = LargeBlob;
	    break;
	case Purged:
	    status = pwrite(fd, "PURG", ORI_OBJECT_TYPESIZE, 0);
	    t = Purged;
	    break;
	default:
	    printf("Unknown object type!\n");
	    assert(false);
	    break;
    }
    if (status < 0)
	return -errno;

    assert(status == 4);

    this->flags = flags;
    //checkFlags();
    status = pwrite(fd, &flags,
		    ORI_OBJECT_FLAGSSIZE, 4);
    if (status < 0)
        return -errno;

    for (int i = 1; i <= 2; i++) {
        status = pwrite(fd, "\0\0\0\0\0\0\0\0",
                ORI_OBJECT_SIZE, 8*i);
        if (status < 0)
            return -errno;
        assert(status == ORI_OBJECT_SIZE);
    }

    return 0;
}

/*
 * Open an existing object read-only.
 */
int
Object::open(const string &path)
{
    int status;
    char buf[5];

    objPath = path;

    fd = ::open(path.c_str(), O_RDWR);
    if (fd < 0)
	return -errno;

    buf[4] = '\0';
    status = pread(fd, buf, ORI_OBJECT_TYPESIZE, 0);
    if (status < 0) {
	::close(fd);
	fd = -1;
	return -errno;
    }

    assert(status == ORI_OBJECT_TYPESIZE);

    if (strcmp(buf, "CMMT") == 0) {
        t = Commit;
    } else if (strcmp(buf, "TREE") == 0) {
        t = Tree;
    } else if (strcmp(buf, "BLOB") == 0) {
        t = Blob;
    } else if (strcmp(buf, "LGBL") == 0) {
        t = LargeBlob;
    } else if (strcmp(buf, "PURG") == 0) {
        t = Purged;
    } else {
        printf("Unknown object type!\n");
        assert(false);
    }

    status = pread(fd, (void *)&flags, ORI_OBJECT_FLAGSSIZE, 4);
    if (status < 0) {
        close();
        return -errno;
    }
    //checkFlags();

    status = pread(fd, (void *)&len, ORI_OBJECT_SIZE, 8);
    if (status < 0) {
	::close(fd);
	fd = -1;
	t = Null;
	return -errno;
    }

    status = pread(fd, (void *)&storedLen, ORI_OBJECT_SIZE, 16);
    if (status < 0) {
        close();
        return -errno;
    }

    return 0;
}

/*
 * Close the object file.
 */
void
Object::close()
{
    if (fd != -1)
        ::close(fd);

    fd = -1;
    t = Null;
    len = -1;
    storedLen = -1;
    objPath = "";
}

/*
 * Read the object type.
 */
Object::Type
Object::getType()
{
    return t;
}

/*
 * Get the on-disk file size including the object header.
 */
size_t
Object::getDiskSize()
{
    struct stat sb;

    if (fstat(fd, &sb) < 0) {
	return -errno;
    }

    return sb.st_size;
}

/*
 * Get the extracted object size.
 */
size_t
Object::getObjectSize()
{
    return len;
}

size_t
Object::getObjectStoredSize() {
    return storedLen;
}

/* Flags */

bool Object::getCompressed() {
    return flags & ORI_FLAG_COMPRESSED;
}

#define COPYFILE_BUFSZ	4096

/*
 * Purge object.
 */
int
Object::purge()
{
    int status;

    // XXX: Support for backrefs and large blobs
    NOT_IMPLEMENTED(ORI_OBJECT_HDRSIZE + len == getDiskSize());
    NOT_IMPLEMENTED(t == Blob);

    status = pwrite(fd, "PURG", ORI_OBJECT_TYPESIZE, 0);
    t = Purged;
    if (status < 0)
	return -errno;

    status = ftruncate(fd, ORI_OBJECT_HDRSIZE);
    if (status < 0)
	return -errno;

    return 0;
}

/*
 * Append the specified file into the object.
 */
int
Object::appendFile(const string &path)
{
    int srcFd;
    char buf[COPYFILE_BUFSZ];
    struct stat sb;
    int64_t bytesLeft;
    int64_t bytesRead, bytesWritten;

    lzma_stream strm = LZMA_STREAM_INIT;
    if (getCompressed()) setupLzma(&strm, true);

    if (lseek(fd, ORI_OBJECT_HDRSIZE, SEEK_SET) != ORI_OBJECT_HDRSIZE) {
        return -errno;
    }

    srcFd = ::open(path.c_str(), O_RDONLY);
    if (srcFd < 0)
        return -errno;

    if (fstat(srcFd, &sb) < 0) {
	::close(srcFd);
	return -errno;
    }

    len = sb.st_size;
    if (pwrite(fd, (void *)&len, ORI_OBJECT_SIZE, 8) != ORI_OBJECT_SIZE) {
	::close(srcFd);
	return -errno;
    }

    bytesLeft = sb.st_size;
    while(bytesLeft > 0) {
        bytesRead = read(srcFd, buf, MIN(bytesLeft, COPYFILE_BUFSZ));
        if (bytesRead < 0) {
            if (errno == EINTR)
            continue;
            goto error;
        }

        if (getCompressed()) {
            strm.next_in = (uint8_t*)buf;
            strm.avail_in = bytesRead;
            appendLzma(fd, &strm, LZMA_RUN);
        }
        else {
retryWrite:
            bytesWritten = write(fd, buf, bytesRead);
            if (bytesWritten < 0) {
                if (errno == EINTR)
                    goto retryWrite;
                goto error;
            }

            // XXX: Need to handle this case!
            assert(bytesRead == bytesWritten);
        }

        bytesLeft -= bytesRead;
    }

    // Write final (post-compression) size
    if (getCompressed()) {
        appendLzma(fd, &strm, LZMA_FINISH);
        storedLen = strm.total_out;
    }
    else {
        storedLen = len;
    }

    if (pwrite(fd, (void *)&storedLen, ORI_OBJECT_SIZE, 16) != ORI_OBJECT_SIZE)
        return -errno;

    ::close(srcFd);
    return sb.st_size;

error:
    ::close(srcFd);
    return -errno;
}

/*
 * Extract the contents of the object file into the specified path.
 */
int
Object::extractFile(const string &path)
{
    int dstFd;
    char buf[COPYFILE_BUFSZ];
    int64_t bytesLeft;
    int64_t bytesRead, bytesWritten;

    lzma_stream strm = LZMA_STREAM_INIT;
    if (getCompressed()) setupLzma(&strm, false);

    if (lseek(fd, ORI_OBJECT_HDRSIZE, SEEK_SET) != ORI_OBJECT_HDRSIZE) {
        return -errno;
    }

    dstFd = ::open(path.c_str(), O_WRONLY | O_CREAT,
		   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (dstFd < 0) {
	return -errno;
    }

    bytesLeft = storedLen;
    while(bytesLeft > 0) {
        bytesRead = read(fd, buf, MIN(bytesLeft, COPYFILE_BUFSZ));
        if (bytesRead < 0) {
            if (errno == EINTR)
            continue;
            goto error;
        }

        if (getCompressed()) {
            strm.next_in = (uint8_t*)buf;
            strm.avail_in = bytesRead;
            appendLzma(dstFd, &strm, LZMA_RUN);
        }
        else {
retryWrite:
            bytesWritten = write(dstFd, buf, bytesRead);
            if (bytesWritten < 0) {
                if (errno == EINTR)
                goto retryWrite;
                goto error;
            }

            // XXX: Need to handle this case!
            assert(bytesRead == bytesWritten);
        }

        bytesLeft -= bytesRead;
    }

    if (getCompressed()) {
        appendLzma(dstFd, &strm, LZMA_FINISH);
        assert(strm.total_out == (size_t)len);
    }

    ::close(dstFd);
    return len; // TODO

error:
    unlink(path.c_str());
    ::close(dstFd);
    return -errno;
}

/*
 * Append a blob to the object file.
 */
int
Object::appendBlob(const string &blob)
{
    int status;

    len = blob.length();
    if (pwrite(fd, (void *)&len, ORI_OBJECT_SIZE, 8) != ORI_OBJECT_SIZE) {
        return -errno;
    }

    if (getCompressed()) {
        if (lseek(fd, ORI_OBJECT_HDRSIZE, SEEK_SET) != ORI_OBJECT_HDRSIZE)
            return -errno;

        lzma_stream strm = LZMA_STREAM_INIT;
        setupLzma(&strm, true);

        strm.next_in = (const uint8_t *)blob.data();
        strm.avail_in = blob.length();
        appendLzma(fd, &strm, LZMA_RUN);
        appendLzma(fd, &strm, LZMA_FINISH);

        storedLen = strm.total_out;
    }
    else {
        status = pwrite(fd, blob.data(), blob.length(), ORI_OBJECT_HDRSIZE);
        if (status < 0)
            return -errno;

        assert(status == blob.length());

        storedLen = blob.length();
    }

    status = pwrite(fd, (void *)&storedLen, ORI_OBJECT_SIZE, 16);
    if (status < 0)
        return -errno;

    return 0;
}

/*
 * Extract the blob from the object file.
 */
string
Object::extractBlob()
{
    int status;
    size_t length = getObjectSize();
    char *buf;
    string rval;

    assert(length >= 0);

    buf = new char[length];

    if (getCompressed()) {
        if (lseek(fd, ORI_OBJECT_HDRSIZE, SEEK_SET) != ORI_OBJECT_HDRSIZE)
            return "";

        lzma_stream strm = LZMA_STREAM_INIT;
        setupLzma(&strm, false);
        strm.next_out = (uint8_t*)buf;
        strm.avail_out = length;

        uint8_t in_buf[COPYFILE_BUFSZ];

        off_t curr_off = 0;
        while (curr_off < storedLen) {
            ssize_t read_bytes = read(fd, in_buf, MIN(COPYFILE_BUFSZ, storedLen - curr_off));
            if (read_bytes < 1) {
                return "";
            }

            strm.next_in = in_buf;
            strm.avail_in = read_bytes;

            // TODO is this loop necessary?
            while (strm.avail_in > 0) {
                lzma_ret ret_xz = lzma_code(&strm, LZMA_RUN);
                if (ret_xz == LZMA_STREAM_END) break;
            }

            curr_off += read_bytes;
        }

        lzma_ret ret_xz = lzma_code(&strm, LZMA_FINISH);
        if (ret_xz != LZMA_STREAM_END)
            return "";
    }
    else {
        status = pread(fd, buf, length, ORI_OBJECT_HDRSIZE);
        if (status < 0)
            return "";
        assert(status == length);
    }

    rval.assign(buf, length);

    return rval;
}

/*
 * Recompute the SHA-256 hash to verify the file.
 */
string
Object::computeHash()
{
    char buf[COPYFILE_BUFSZ];
    int64_t bytesLeft;
    int64_t bytesRead;
    SHA256_CTX state;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    stringstream rval;

    if (lseek(fd, ORI_OBJECT_HDRSIZE, SEEK_SET) != ORI_OBJECT_HDRSIZE) {
	return "";
    }

    SHA256_Init(&state);

    bytesLeft = getObjectSize();
    while(bytesLeft > 0) {
	bytesRead = read(fd, buf, MIN(bytesLeft, COPYFILE_BUFSZ));
	if (bytesRead < 0) {
	    if (errno == EINTR)
		continue;
	    return "";
	}

	SHA256_Update(&state, buf, bytesRead);
	bytesLeft -= bytesRead;
    }

    SHA256_Final(hash, &state);

    // Convert into string.
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
	rval << hex << setw(2) << setfill('0') << (int)hash[i];
    }

    return rval.str();
}


/*
 * Metadata operations
 */

#define OFF_MD_HASH (ORI_OBJECT_HDRSIZE + getObjectStoredSize())

void Object::addMetadataEntry(MdType type, const std::string &data) {
    off_t offset = getDiskSize();
    int err = pwrite(fd, _getIdForMdType(type), 2, offset);
    assert(err == 2);
    uint32_t len = data.length();
    err = pwrite(fd, &len, 4, offset + 2);
    assert(err = 4);
    err = pwrite(fd, data.data(), data.length(), offset + 6);
    assert((size_t)err == data.length());
    fsync(fd);

    std::string hash = computeMetadataHash();
    assert(hash != "");
    err = pwrite(fd, hash.data(), ORI_MD_HASHSIZE, OFF_MD_HASH);
    assert(err == ORI_MD_HASHSIZE);
}

std::string Object::computeMetadataHash() {
    off_t offset = OFF_MD_HASH + ORI_MD_HASHSIZE;
    if (lseek(fd, offset, SEEK_SET) != offset) {
        return "";
    }

    SHA256_CTX state;
    SHA256_Init(&state);

    if (getDiskSize() < offset)
        return "";

    size_t bytesLeft = getDiskSize() - offset;
    while(bytesLeft > 0) {
        uint8_t buf[COPYFILE_BUFSZ];
        int bytesRead = read(fd, buf, MIN(bytesLeft, COPYFILE_BUFSZ));
        if (bytesRead < 0) {
            if (errno == EINTR)
                continue;
            return "";
        }

        SHA256_Update(&state, buf, bytesRead);
        bytesLeft -= bytesRead;
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &state);

    std::string rval;
    rval.assign((char *)hash, SHA256_DIGEST_LENGTH);
    return rval;
}



void
Object::addBackref(const string &objId, Object::BRState state)
{
    string buf = objId;

    assert(objId.length() == 2 * SHA256_DIGEST_LENGTH);
    assert(state == BRRef || state == BRPurged);

    if (state == BRRef) {
        buf += "R";
    }
    if (state == BRPurged) {
        buf += "P";
    }

    addMetadataEntry(MdBackref, buf);
}

void
Object::updateBackref(const string &objId, Object::BRState state)
{
    int status;
    map<string, BRState> backrefs;
    map<string, BRState>::iterator it;

    assert(objId.length() == SHA256_DIGEST_LENGTH);
    assert(state == BRRef || state == BRPurged);

    backrefs = getBackref();
    backrefs[objId] = state;

    /*
     * XXX: Crash Recovery
     *
     * Either we can log here to make crash recovery easier, otherwise
     * we should just write the single modified byte.  That should always
     * translate to a single sector write, which is atomic.
     */

    status = ftruncate(fd, ORI_OBJECT_HDRSIZE + len);
    assert(status == 0);

    for (it = backrefs.begin(); it != backrefs.end(); it++) {
	addBackref((*it).first, (*it).second);
    }
}

void
Object::clearBackref()
{
    int status;

    status = ftruncate(fd, ORI_OBJECT_HDRSIZE + len);
    assert(status == 0);
}

map<string, Object::BRState>
Object::getBackref()
{
    int status;
    size_t backrefSize;
    int off;
    char *buf;
    map<string, BRState> rval;

    backrefSize = getDiskSize() - ORI_OBJECT_HDRSIZE - len;
    buf = new char[backrefSize];
    status = pread(fd, buf, backrefSize, ORI_OBJECT_HDRSIZE + len);
    assert(status == backrefSize);

    for (off = 0; off < backrefSize; off++) {
        string objId;

        objId.assign(buf+off, 2 * SHA256_DIGEST_LENGTH);
        off += 2 * SHA256_DIGEST_LENGTH;

        if (buf[off] == 'R') {
            rval[objId] = BRRef;
        } else if (buf[off] == 'P') {
            rval[objId] = BRPurged;
        } else {
            assert(false);
        }
    }

    return rval;
}



/*
 * Private methods
 */

// Code from xz_pipe_comp.c in xz examples

void Object::setupLzma(lzma_stream *strm, bool encode) {
    lzma_ret ret_xz;
    if (encode) {
        ret_xz = lzma_easy_encoder(strm, 2, LZMA_CHECK_NONE);
    }
    else {
        ret_xz = lzma_stream_decoder(strm, UINT64_MAX, 0);
    }
    assert(ret_xz == LZMA_OK);
}

bool Object::appendLzma(int dstFd, lzma_stream *strm, lzma_action action) {
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

const char *Object::_getIdForMdType(MdType type) {
    switch (type) {
    case MdBackref:
        return "BR";
    }

    return NULL;
}
