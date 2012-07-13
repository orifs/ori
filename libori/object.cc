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
#include "object.h"

using namespace std;

#define COPYFILE_BUFSZ	(256 * 1024)

/*
 * ObjectInfo
 */
ObjectInfo::ObjectInfo()
    : type(Object::Null), flags(0), payload_size((size_t)-1)
{
    hash.resize(SHA256_DIGEST_LENGTH*2);
}

ObjectInfo::ObjectInfo(const char *in_hash)
    : type(Object::Null), flags(0), payload_size(0)
{
    hash.resize(SHA256_DIGEST_LENGTH*2);
    memcpy(&hash[0], in_hash, hash.size());
}

std::string
ObjectInfo::getInfo() const
{
    string rval;
    char buf[16];
    const char *type_str = Object::getStrForType(type);
    if (type_str == NULL) {
        assert(false);
        return "";
    }

    strncpy(buf, type_str, ORI_OBJECT_TYPESIZE);
    memcpy(buf+4, &flags, ORI_OBJECT_FLAGSSIZE);
    memcpy(buf+8, &payload_size, ORI_OBJECT_SIZE);

    rval.assign(buf, 16);

    return rval;
}

void
ObjectInfo::setInfo(const std::string &info)
{
    char buf[16];

    assert(info.size() == 16);
    memcpy(buf, info.c_str(), 16);

    memcpy(&flags, buf+4, ORI_OBJECT_FLAGSSIZE);
    memcpy(&payload_size, buf+8, ORI_OBJECT_SIZE);
    buf[4] = '\0';
    type = getTypeForStr(buf);
    assert(type != Object::Null);
}

ssize_t ObjectInfo::writeTo(int fd, bool seekable) {
    const char *type_str = Object::getStrForType(type);
    if (type_str == NULL)
        return -1;

    ssize_t status;
    if (seekable) {
        status = pwrite(fd, type_str, ORI_OBJECT_TYPESIZE, 0);
        if (status < 0) return status;
        assert(status == ORI_OBJECT_TYPESIZE);

        status = pwrite(fd, &flags, ORI_OBJECT_FLAGSSIZE, 4);
        if (status < 0) return status;
        assert(status == ORI_OBJECT_FLAGSSIZE);

        status = pwrite(fd, &payload_size, ORI_OBJECT_SIZE, 8);
        if (status < 0) return status;
        assert(status == ORI_OBJECT_SIZE);

        return 0;
    }
    else {
        // TODO!!! use write() instead
        assert(false);
    }
}

bool
ObjectInfo::hasAllFields() const
{
    if (type == Object::Null)
        return false;
    if (hash == "")
        return false; // hash shouldn't be all zeros
    if (payload_size == (size_t)-1)
        return false; // no objects should be that large, due to LargeBlob
    return true;
}

bool ObjectInfo::operator <(const ObjectInfo &other) const {
    if (hash < other.hash) return true;
    if (type < other.type) return true;
    if (flags < other.flags) return true;
    if (payload_size < other.payload_size) return true;
    return false;
}

bool ObjectInfo::getCompressed() const {
    return flags & ORI_FLAG_COMPRESSED;
}



/*
 * Object
 */
const char *Object::getStrForType(Type type) {
    const char *type_str = NULL;
    switch (type) {
        case Commit:    type_str = "CMMT"; break;
        case Tree:      type_str = "TREE"; break;
        case Blob:      type_str = "BLOB"; break;
        case LargeBlob: type_str = "LGBL"; break;
        case Purged:    type_str = "PURG"; break;
        default:
            printf("Unknown object type!\n");
            assert(false);
            return NULL;
    }
    return type_str;
}

Object::Type Object::getTypeForStr(const char *str) {
    if (strcmp(str, "CMMT") == 0) {
        return Commit;
    }
    else if (strcmp(str, "TREE") == 0) {
        return Tree;
    }
    else if (strcmp(str, "BLOB") == 0) {
        return Blob;
    }
    else if (strcmp(str, "LGBL") == 0) {
        return LargeBlob;
    }
    else if (strcmp(str, "PURG") == 0) {
        return Purged;
    }
    return Null;
}





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
    char buf[5];

    objPath = path;

    fd = ::open(path.c_str(), O_RDWR);
    if (fd < 0) {
        perror("open");
	return -errno;
    }

    buf[4] = '\0';
    status = pread(fd, buf, ORI_OBJECT_TYPESIZE, 0);
    if (status < 0) {
        int en = errno;
        perror("pread type");
	close();
	return -en;
    }

    assert(status == ORI_OBJECT_TYPESIZE);

    info.type = getTypeForStr(buf);
    if (info.type == Null) {
        printf("Unknown object type!\n");
        assert(false);
    }

    status = pread(fd, (void *)&info.flags, ORI_OBJECT_FLAGSSIZE, 4);
    if (status < 0) {
        int en = errno;
        perror("pread flags");
        close();
        return -en;
    }

    status = pread(fd, (void *)&info.payload_size, ORI_OBJECT_SIZE, 8);
    if (status < 0) {
        int en = errno;
        perror("pread payload_size");
	close();
	return -en;
    }

    status = pread(fd, (void *)&storedLen, ORI_OBJECT_SIZE, 16);
    if (status < 0) {
        int en = errno;
        perror("pread storedLen");
        close();
        return -en;
    }

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
    assert(fileSize == sb.st_size);
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
 * Metadata operations
 */

#define OFF_MD_HASH (ORI_OBJECT_HDRSIZE + getStoredPayloadSize())

/* Organization of metadata on disk: immediately following the stored data, the
 * SHA256 checksum of all the metadata (size ORI_MD_HASHSIZE); following that, a
 * plain unpadded array of metadata entries. Each entry begins with a 2-byte
 * identifier followed by 2 bytes denoting the length in bytes of the entry
 * followed by the entry itself.
 */
void LocalObject::addMetadataEntry(MdType type, const std::string &data) {
    assert(checkMetadata());

    off_t offset = getFileSize();
    if (offset == (off_t)OFF_MD_HASH) {
        offset += ORI_MD_HASHSIZE;
        fileSize += ORI_MD_HASHSIZE;
    }

    int err = pwrite(fd, _getIdForMdType(type), 2, offset);
    assert(err == 2);
    assert(data.length() <= UINT16_MAX);
    uint16_t len = data.length();
    err = pwrite(fd, &len, 2, offset + 2);
    assert(err == 2);

    err = pwrite(fd, data.data(), data.length(), offset + 4);
    assert((size_t)err == data.length());
    fsync(fd);

    fileSize += 4 + data.length();

    std::string hash = computeMetadataHash();
    assert(hash != "");
    err = pwrite(fd, hash.data(), ORI_MD_HASHSIZE, OFF_MD_HASH);
    assert(err == ORI_MD_HASHSIZE);
}

std::string LocalObject::computeMetadataHash() {
    off_t offset = OFF_MD_HASH + ORI_MD_HASHSIZE;
    if (lseek(fd, offset, SEEK_SET) != offset) {
        return "";
    }

    SHA256_CTX state;
    SHA256_Init(&state);

    if ((off_t)getFileSize() < offset)
        return "";

    size_t bytesLeft = getFileSize() - offset;
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

bool
LocalObject::checkMetadata()
{
    if (getFileSize() <= OFF_MD_HASH) {
        // No metadata to check
        return true;
    }

    char disk_hash[ORI_MD_HASHSIZE+1];
    int status = pread(fd, disk_hash, ORI_MD_HASHSIZE, OFF_MD_HASH);
    if (status < 0) {
        perror("pread");
        return false;
    }
    disk_hash[ORI_MD_HASHSIZE] = '\0';

    std::string computed_hash = computeMetadataHash();

    if (strcmp(computed_hash.c_str(), disk_hash) == 0)
        return true;
    return false;
}

void
LocalObject::clearMetadata()
{
    int status;

    status = ftruncate(fd, OFF_MD_HASH);
    assert(status == 0);
    fileSize = ORI_OBJECT_HDRSIZE + storedLen;
}


void
LocalObject::addBackref(const string &objId, LocalObject::BRState state)
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
LocalObject::updateBackref(const string &objId, LocalObject::BRState state)
{
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

    clearMetadata(); // was clearBackref

    for (it = backrefs.begin(); it != backrefs.end(); it++) {
	addBackref((*it).first, (*it).second);
    }
}

map<string, LocalObject::BRState>
LocalObject::getBackref()
{
    map<string, BRState> rval;

    off_t md_off = OFF_MD_HASH + ORI_MD_HASHSIZE;
    if ((off_t)getFileSize() < md_off) {
        // No metadata
        return rval;
    }

    // Load all metadata into memory
    size_t backrefSize = getFileSize() - md_off;

    std::vector<uint8_t> buf;
    buf.resize(backrefSize);
    int status = pread(fd, &buf[0], backrefSize, md_off);
    assert(status == backrefSize);

    // XXX: more generic way to iterate over metadata
    uint8_t *ptr = &buf[0];
    while ((ptr - &buf[0]) < backrefSize) {
        MdType md_type = _getMdTypeForStr((const char *)ptr);
        size_t md_len = *((uint16_t*)(ptr+2));

        ptr += 4;
        assert(ptr + md_len <= &buf[0] + backrefSize);

        if (md_type == MdBackref) {
            assert(md_len == 2*SHA256_DIGEST_LENGTH + 1);

            string objId;
            objId.assign((char *)ptr, 2*SHA256_DIGEST_LENGTH);

            char ref_type = *(ptr + 2*SHA256_DIGEST_LENGTH);
            if (ref_type == 'R') {
                rval[objId] = BRRef;
            } else if (ref_type == 'P') {
                rval[objId] = BRPurged;
            } else {
                assert(false);
            }
        }

        ptr += md_len;
    }

    return rval;
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

const char *LocalObject::_getIdForMdType(MdType type) {
    switch (type) {
    case MdBackref:
        return "BR";
    default:
        return NULL;
    }
}

LocalObject::MdType LocalObject::_getMdTypeForStr(const char *str) {
    if (str[0] == 'B') {
        if (str[1] == 'R') {
            return MdBackref;
        }
    }
    return MdNull;
}
