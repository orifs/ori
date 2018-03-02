/*
 * Copyright (c) 2012-2013 Stanford University
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

#include <cassert>
#include <string.h>
#include <stdlib.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#endif

#ifdef ORI_USE_FASTLZ
#include "fastlz.h"
#endif /* ORI_USE_FASTLZ */

#include <string>

#include "tuneables.h"
#include "byteswap.h"

#include <oriutil/debug.h>
#include <oriutil/systemexception.h>
#include <oriutil/stream.h>

using namespace std;

#define USE_EXCEPTIONS 1

/*
 * Stream Types
 */
#define STREAM_TYPE_INT8        0xA0
#define STREAM_TYPE_INT16       0xA1
#define STREAM_TYPE_INT32       0xA2
#define STREAM_TYPE_INT64       0xA3
#define STREAM_TYPE_UINT8       0xA4
#define STREAM_TYPE_UINT16      0xA5
#define STREAM_TYPE_UINT32      0xA6
#define STREAM_TYPE_UINT64      0xA7
#define STREAM_TYPE_PSTR        0xA8
#define STREAM_TYPE_LPSTR       0xA9
#define STREAM_TYPE_OBJHASH     0xAA
#define STREAM_TYPE_OBJINFO     0xAB

/*
 * basestream
 */
// Error handling

void basestream::setErrno(const char *msg) {
    char buf[512];
    snprintf(buf, 512, "%s: %s (%d)", msg, strerror(errno), errno);
    last_error.assign(buf);
    last_errnum = errno;
#if DEBUG
    LOG("Stream error: %s", buf);
#endif
}

bool basestream::inheritError(basestream *bs) {
    if (bs->error()) {
        last_error.assign(bs->error());
        last_errnum = bs->errnum();
        return true;
    }
    return false;
}



/*
 * bytestream
 */
void bytestream::enableTypes()
{
    typedStream = true;
}

void bytestream::disableTypes()
{
    typedStream = false;
}

bool bytestream::isTyped()
{
    return typedStream;
}

bool bytestream::readExact(uint8_t *buf, size_t n)
{
    size_t total = 0;
    while (total < n) {
        size_t readBytes = read(buf+total, n-total);
        if (error()) return false;
        if (readBytes == 0) {
            throw std::ios_base::failure("End of stream");
        }
        total += readBytes;
    }

    return true;
}

std::string bytestream::readAll() {
    std::string rval;

    if (sizeHint() == 0) {
        // Need to read to end
        uint8_t buf[COPYFILE_BUFSZ];
        while (!ended()) {
            size_t n = read(buf, COPYFILE_BUFSZ);
            if (error()) {
                return "";
            }

            size_t oldSize = rval.size();
            rval.resize(oldSize + n);
            memcpy(&rval[oldSize], buf, n);
        }
        return rval;
    }
    else {
        rval.resize(sizeHint());
        readExact((uint8_t*)&rval[0], sizeHint());
        if (error()) {
            return "";
        }
        return rval;
    }
}

int bytestream::copyToFile(const std::string &path) {
    int dstFd = ::open(path.c_str(), O_WRONLY | O_CREAT,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (dstFd < 0)
        return -errno;

    int rval = copyToFd(dstFd);
    if (rval < 0) {
        ::close(dstFd);
        unlink(path.c_str());
        return rval;
    }
    ::close(dstFd);
    return rval;
}

int bytestream::copyToFd(int dstFd)
{
    size_t totalWritten = 0;
    uint8_t buf[COPYFILE_BUFSZ];
    while (!ended()) {
        size_t bytesRead = read(buf, COPYFILE_BUFSZ);
        if (error()) return -errnum();
retryWrite:
        ssize_t bytesWritten = write(dstFd, buf, bytesRead);
        if (bytesWritten < 0) {
            if (errno == EINTR)
                goto retryWrite;
            return -errno;
        }
        totalWritten += bytesWritten;
    }

    if (sizeHint() > 0)
        assert(totalWritten == sizeHint());
    return totalWritten;
}

// High-level I/O
int bytestream::readPStr(std::string &out)
{
    try {
        if (typedStream) {
            uint8_t type;
            bool success = readExact(&type, 1);
            if (!success) return 0;
            if (type != STREAM_TYPE_PSTR) {
                ASSERT(false);
                throw exception();
            }
        }
        uint8_t len = readUInt8();
        out.resize(len);
        bool success = readExact((uint8_t*)&out[0], len);
        if (!success) return 0;
        return len;
    }
    catch (std::ios_base::failure &e)
    {
        return 0;
    }
}

int bytestream::readLPStr(std::string &out)
{
    try {
        if (typedStream) {
            uint8_t type;
            bool success = readExact(&type, 1);
            if (!success) return 0;
            if (type != STREAM_TYPE_LPSTR) {
                ASSERT(false);
                throw exception();
            }
        }
        uint16_t len = readUInt16();
        out.resize(len);
        bool success = readExact((uint8_t*)&out[0], len);
        if (!success) return 0;
        return len;
    }
    catch (std::ios_base::failure &e)
    {
        return 0;
    }
}

void bytestream::readHash(ObjectHash &out)
{
    if (typedStream) {
        uint8_t type;
        bool success = readExact(&type, 1);
        if (!success) throw exception();
        if (type != STREAM_TYPE_OBJHASH) {
            ASSERT(false);
            throw exception();
        }
    }
    readExact((uint8_t*)out.hash, ObjectHash::SIZE);
}

void bytestream::readInfo(ObjectInfo &out)
{
    if (typedStream) {
        uint8_t type;
        bool success = readExact(&type, 1);
        if (!success) throw exception();
        if (type != STREAM_TYPE_OBJINFO) {
            ASSERT(false);
            throw exception();
        }
    }
    std::string info_str(ObjectInfo::SIZE, '\0');
    readExact((uint8_t*)&info_str[0], ObjectInfo::SIZE);
    out.fromString(info_str);
}

int8_t
bytestream::readInt8()
{
    if (typedStream) {
        uint8_t type;
        bool success = readExact(&type, 1);
        if (!success) throw exception();
        if (type != STREAM_TYPE_INT8) {
            ASSERT(false);
            throw exception();
        }
    }
    int8_t rval;
    bool success UNUSED = readExact((uint8_t*)&rval, sizeof(int8_t));
    ASSERT(success);
    return rval;
}

int16_t
bytestream::readInt16()
{
    if (typedStream) {
        uint8_t type;
        bool success = readExact(&type, 1);
        if (!success) throw exception();
        if (type != STREAM_TYPE_INT16) {
            ASSERT(false);
            throw exception();
        }
    }
    int16_t rval;
    bool success UNUSED = readExact((uint8_t*)&rval, sizeof(int16_t));
    ASSERT(success);
    return be16toh(rval);
}

int32_t
bytestream::readInt32()
{
    if (typedStream) {
        uint8_t type;
        bool success = readExact(&type, 1);
        if (!success) throw exception();
        if (type != STREAM_TYPE_INT32) {
            ASSERT(false);
            throw exception();
        }
    }
    int32_t rval;
    bool success UNUSED = readExact((uint8_t*)&rval, sizeof(int32_t));
    ASSERT(success);
    return be32toh(rval);
}

int64_t
bytestream::readInt64()
{
    if (typedStream) {
        uint8_t type;
        bool success = readExact(&type, 1);
        if (!success) throw exception();
        if (type != STREAM_TYPE_INT64) {
            ASSERT(false);
            throw exception();
        }
    }
    int64_t rval;
    bool success UNUSED = readExact((uint8_t*)&rval, sizeof(int64_t));
    ASSERT(success);
    return be64toh(rval);
}

uint8_t
bytestream::readUInt8()
{
    if (typedStream) {
        uint8_t type;
        bool success = readExact(&type, 1);
        if (!success) throw exception();
        if (type != STREAM_TYPE_UINT8) {
            ASSERT(false);
            throw exception();
        }
    }
    uint8_t rval;
    bool success UNUSED = readExact((uint8_t*)&rval, sizeof(uint8_t));
    ASSERT(success);
    return rval;
}

uint16_t
bytestream::readUInt16()
{
    if (typedStream) {
        uint8_t type;
        bool success = readExact(&type, 1);
        if (!success) throw exception();
        if (type != STREAM_TYPE_UINT16) {
            ASSERT(false);
            throw exception();
        }
    }
    uint16_t rval;
    bool success UNUSED = readExact((uint8_t*)&rval, sizeof(uint16_t));
    ASSERT(success);
    return be16toh(rval);
}

uint32_t
bytestream::readUInt32()
{
    if (typedStream) {
        uint8_t type;
        bool success = readExact(&type, 1);
        if (!success) throw exception();
        if (type != STREAM_TYPE_UINT32) {
            ASSERT(false);
            throw exception();
        }
    }
    uint32_t rval;
    bool success UNUSED = readExact((uint8_t*)&rval, sizeof(uint32_t));
    ASSERT(success);
    return be32toh(rval);
}

uint64_t
bytestream::readUInt64()
{
    if (typedStream) {
        uint8_t type;
        bool success = readExact(&type, 1);
        if (!success) throw exception();
        if (type != STREAM_TYPE_UINT64) {
            ASSERT(false);
            throw exception();
        }
    }
    uint64_t rval;
    bool success UNUSED = readExact((uint8_t*)&rval, sizeof(uint64_t));
    ASSERT(success);
    return be64toh(rval);
}


/*
 * strstream
 */
strstream::strstream(const std::string &in_buf, size_t start)
    : buf(in_buf), off(start), len(in_buf.size()-start)
{
    assert(start <= in_buf.size());
}

bool strstream::ended() {
    return off >= buf.size();
}

size_t strstream::read(uint8_t *out, size_t n)
{
    size_t left = buf.size() - off;
    size_t to_read = MIN(n, left);
    memcpy(out, &buf[off], to_read);
    off += to_read;
    return to_read;
}

size_t strstream::sizeHint() const
{
    return len;
}

/*
 * fdstream
 */

fdstream::fdstream(int fd, off_t offset, size_t length)
    : fd(fd), length(length), left(length)
{
    if (offset >= 0 && lseek(fd, offset, SEEK_SET) != offset) {
        setErrno("lseek");
    }
}

bool fdstream::ended() {
    return left == 0 || error();
}

size_t fdstream::read(uint8_t *buf, size_t n) {
    size_t final_size = MIN(n, left);
retry_read:
    ssize_t read_bytes = ::read(fd, buf, final_size);
    if (read_bytes < 0) {
        if (errno == EINTR)
            goto retry_read;
        setErrno("read");
        return 0;
    }
    else if (read_bytes == 0) {
        left = 0;
        return 0;
    }
    left -= read_bytes;

    /*LOG("Readd %lu bytes (actually %ld) (%d)\n", n, read_bytes, fd);
    if (n < 100) {
        for (size_t i = 0; i < n; i++)
            LOG("%02x ", ((uint8_t*)buf)[i]);
    }*/

    return read_bytes;
}

size_t fdstream::sizeHint() const {
    if (length != (size_t)-1)
        return length;
    return 0;
}

/*
 * diskstream
 */

diskstream::diskstream(const std::string &filename)
    : fd(-1)
{
    fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
        setErrno("open");
        LOG("open failed: %s", filename.c_str());
        if (USE_EXCEPTIONS)
            throw std::ios_base::failure("Couldn't open file");
        return;
    }

    size_t length = lseek(fd, 0, SEEK_END);
    source.reset(new fdstream(fd, 0, length));
}

diskstream::~diskstream() {
    if (fd > 0)
        close(fd);
}

bool diskstream::ended() {
    return source->ended();
}

size_t diskstream::read(uint8_t *buf, size_t n) {
    return source->read(buf, n);
}

size_t diskstream::sizeHint() const {
    return source->sizeHint();
}

#ifdef ORI_USE_LZMA

/*
 * LZMA zipstream
 */

zipstream::zipstream(bytestream *source, bool compress, size_t size_hint)
    : source(source), size_hint(size_hint), output_ended(false)
{
    assert(source != NULL);

    lzma_stream strm2 = LZMA_STREAM_INIT;
    memcpy(&strm, &strm2, sizeof(lzma_stream));

    if (compress) {
        lzma_ret ret = lzma_easy_encoder(&strm, 0, LZMA_CHECK_NONE);
        if (ret != LZMA_OK)
            setLzmaErr("lzma_easy_encoder", ret);
    }
    else {
        lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, 0);
        if (ret != LZMA_OK)
            setLzmaErr("lzma_stream_decoder", ret);
    }
}

zipstream::~zipstream() {
    delete source;
}

bool zipstream::ended() {
    return output_ended || error();
}

size_t zipstream::read(uint8_t *buf, size_t n) {
    if (output_ended) return 0;

    lzma_action action = source->ended() ? LZMA_FINISH : LZMA_RUN;
    size_t begin_total = strm.total_out;

    strm.next_out = buf;
    strm.avail_out = n;
    while (strm.avail_out > 0) {
        if (output_ended) break;

        if (strm.avail_in == 0) {
            size_t read_bytes = source->read(in_buf, COMPFILE_BUFSZ);
            if (inheritError(source)) return 0;
            action = read_bytes == 0 ? LZMA_FINISH : LZMA_RUN;

            strm.next_in = in_buf;
            strm.avail_in = read_bytes;
        }

        lzma_ret ret = lzma_code(&strm, action);
        if (ret == LZMA_STREAM_END) {
            output_ended = true;
            lzma_end(&strm);
        }
        else if (ret != LZMA_OK) {
            setLzmaErr("lzma_code", ret);
            return 0;
        }
    }

    return strm.total_out - begin_total;
}

size_t zipstream::sizeHint() const {
    return size_hint;
}

size_t zipstream::inputConsumed() const {
    return strm.total_in;
}

const char *lzma_ret_str(lzma_ret ret) {
    switch (ret) {
    case LZMA_STREAM_END:
        return "end of stream";
    case LZMA_NO_CHECK:
        return "input stream has no integrity check";
    case LZMA_UNSUPPORTED_CHECK:
        return "cannot calculate the integrity check";
    case LZMA_GET_CHECK:
        return "integrity check available";
    case LZMA_MEM_ERROR:
        return "cannot allocate memory";
    case LZMA_MEMLIMIT_ERROR:
        return "memory usage limit exceeded";
    case LZMA_FORMAT_ERROR:
        return "file format not recognized";
    case LZMA_OPTIONS_ERROR:
        return "invalid or unsupported options";
    case LZMA_DATA_ERROR:
        return "data is corrupt";
    case LZMA_BUF_ERROR:
        return "no progress is possible";
    case LZMA_PROG_ERROR:
        return "programming error";
    default:
        return "unknown";
    }
}

void zipstream::setLzmaErr(const char *msg, lzma_ret ret)
{
    char buf[512];
    snprintf(buf, 512, "lzmastream %s: %s (%d)\n", msg, lzma_ret_str(ret), ret);
    last_error.assign(buf);
    last_errnum = ret;
}

#endif /* ORI_USE_LZMA */

#ifdef ORI_USE_FASTLZ

/*
 * FastLZ zipstream
 */

zipstream::zipstream(bytestream *source, bool compress, size_t size_hint)
    : source(source),
      size_hint(size_hint),

      compress(compress),
      input_processed(false),

      offset(0),
      output_ended(false)
{
    assert(source != NULL);
    if (size_hint > 0) {
        output.resize(size_hint);
    }
}

zipstream::~zipstream() {
    delete source;
}

bool zipstream::ended() {
    return output_ended; // || error();
}

size_t zipstream::read(uint8_t *buf, size_t n) {
    if (output_ended) return 0;

    if (!input_processed) {
        input = source->readAll();
        if (output.size() == 0) {
            NOT_IMPLEMENTED(compress);
            output.resize(input.size() * 1.3);
        }

        int finalSize = 0;
	if (compress) {
	    finalSize = fastlz_compress(&input[0], input.size(), &output[0]);
	    if (finalSize == 0) {
		last_error = "FastLZ couldn't compress";
                return 0;
            }
	} else {
	    finalSize = fastlz_decompress(&input[0], input.size(), &output[0],
                    output.size());
	    if (finalSize == 0) {
		last_error = "FastLZ couldn't decompress";
                return 0;
            }
	}

        output.resize(finalSize);
	input_processed = true;
    }

    size_t to_copy = MIN(n, output.size() - offset);
    memcpy(buf, &output[offset], to_copy);
    offset += to_copy;

    if (offset == output.size())
	output_ended = true;

    return to_copy;
}

size_t zipstream::sizeHint() const {
    return size_hint;
}

size_t zipstream::inputConsumed() const {
    return (size_t)((offset / (float)output.size()) * input.size());
}

#endif /* ORI_USE_FASTLZ */

/*
 * bytewstream
 */
void bytewstream::enableTypes()
{
    typedStream = true;
}

void bytewstream::disableTypes()
{
    typedStream = false;
}

bool bytewstream::isTyped()
{
    return typedStream;
}

void bytewstream::copyFrom(bytestream *bs)
{
    size_t totalWritten = 0;
    uint8_t buf[COPYFILE_BUFSZ];
    while (!bs->ended()) {
        size_t bytesRead = bs->read(buf, COPYFILE_BUFSZ);
        //if (bs->error()) return -bs->errnum();
        ssize_t bytesWritten = bytesRead;
        write(buf, bytesRead);
        // if (error()) return -errnum();
        totalWritten += bytesWritten;
    }

    if (bs->sizeHint() > 0)
        assert(totalWritten == bs->sizeHint());
    //return totalWritten;
}

int bytewstream::writePStr(const std::string &str)
{
    assert(str.size() <= 255);
    uint8_t size = str.size();
    if (typedStream) {
        uint8_t type = STREAM_TYPE_PSTR;
        if (write(&type, 1) != 1)
            return -1;
    }
    if (writeUInt8(size) != sizeof(uint8_t)) {
        return -1;
    }
    ssize_t status = write(str.data(), size);
    if (status < 0) {
        return -1;
    }
    return status + 1;
}

int bytewstream::writeLPStr(const std::string &str)
{
    assert(str.size() <= 65535);
    uint16_t size = str.size();
    if (typedStream) {
        uint8_t type = STREAM_TYPE_LPSTR;
        if (write(&type, 1) != 1)
            return -1;
    }
    if (writeUInt16(size) != sizeof(uint16_t)) {
        return -1;
    }
    ssize_t status = write(str.data(), size);
    if (status < 0) {
        return -1;
    }
    return status + 1;
}

void bytewstream::writeHash(const ObjectHash &hash)
{
    if (typedStream) {
        uint8_t type = STREAM_TYPE_OBJHASH;
        if (write(&type, 1) != 1)
            throw SystemException();
    }
    write(hash.hash, ObjectHash::SIZE);
}

int bytewstream::writeInfo(const ObjectInfo &info)
{
    std::string info_str = info.toString();
    assert(info_str.size() == ObjectInfo::SIZE);
    if (typedStream) {
        uint8_t type = STREAM_TYPE_OBJINFO;
        if (write(&type, 1) != 1)
            return -1;
    }
    return write(&info_str[0], ObjectInfo::SIZE);
}

int
bytewstream::writeInt8(int8_t n)
{
    if (typedStream) {
        uint8_t type = STREAM_TYPE_INT8;
        if (write(&type, 1) != 1)
            return -1;
    }
    return (int)write(&n, sizeof(int8_t));
}

int
bytewstream::writeInt16(int16_t n)
{
    int16_t val = htobe16(n);
    if (typedStream) {
        uint8_t type = STREAM_TYPE_INT16;
        if (write(&type, 1) != 1)
            return -1;
    }
    return (int)write(&val, sizeof(int16_t));
}

int
bytewstream::writeInt32(int32_t n)
{
    int32_t val = htobe32(n);
    if (typedStream) {
        uint8_t type = STREAM_TYPE_INT32;
        if (write(&type, 1) != 1)
            return -1;
    }
    return (int)write(&val, sizeof(int32_t));
}

int
bytewstream::writeInt64(int64_t n)
{
    int64_t val = htobe64(n);
    if (typedStream) {
        uint8_t type = STREAM_TYPE_INT64;
        if (write(&type, 1) != 1)
            return -1;
    }
    return (int)write(&val, sizeof(int64_t));
}

int
bytewstream::writeUInt8(uint8_t n)
{
    if (typedStream) {
        uint8_t type = STREAM_TYPE_UINT8;
        if (write(&type, 1) != 1)
            return -1;
    }
    return (int)write(&n, sizeof(uint8_t));
}

int
bytewstream::writeUInt16(uint16_t n)
{
    uint16_t val = htobe16(n);
    if (typedStream) {
        uint8_t type = STREAM_TYPE_UINT16;
        if (write(&type, 1) != 1)
            return -1;
    }
    return (int)write(&val, sizeof(uint16_t));
}

int
bytewstream::writeUInt32(uint32_t n)
{
    uint32_t val = htobe32(n);
    if (typedStream) {
        uint8_t type = STREAM_TYPE_UINT32;
        if (write(&type, 1) != 1)
            return -1;
    }
    return (int)write(&val, sizeof(uint32_t));
}

int
bytewstream::writeUInt64(uint64_t n)
{
    uint64_t val = htobe64(n);
    if (typedStream) {
        uint8_t type = STREAM_TYPE_UINT64;
        if (write(&type, 1) != 1)
            return -1;
    }
    return (int)write(&val, sizeof(uint64_t));
}

/*
 * strwstream
 */

strwstream::strwstream()
{
}

strwstream::strwstream(const std::string &initial)
    : buf(initial)
{
}

strwstream::strwstream(size_t reserved)
{
    buf.reserve(reserved);
}

ssize_t strwstream::write(const void *bytes, size_t n)
{
    //if (buf.capacity() - buf.size() < n)
    //    buf.reserve(buf.capacity()+n);

    size_t oldSize = buf.size();
    buf.resize(oldSize+n);
    memcpy(&buf[oldSize], bytes, n);
    return n;
}

const std::string &strwstream::str() const
{
    return buf;
}

/*
 * fdwstream
 */
fdwstream::fdwstream(int fd)
    : fd(fd)
{
    assert(fd >= 0);
}

ssize_t fdwstream::write(const void *bytes, size_t n)
{
    size_t totalWritten = 0;
    while (totalWritten < n) {
retryWrite:
        ssize_t bytesWritten = ::write(fd, ((uint8_t*)bytes)+totalWritten, n-totalWritten);
        if (bytesWritten < 0) {
            if (errno == EINTR)
                goto retryWrite;
            setErrno("write");
            return -errno;
        }
        totalWritten += bytesWritten;
    }

    /*fprintf(stderr, "Wrote %lu bytes (%d)\n", n, fd);
    if (n < 100) {
        for (size_t i = 0; i < n; i++)
            fprintf(stderr, "%02x ", ((uint8_t*)bytes)[i]);
        fprintf(stderr, "\n");
    }*/

    assert(totalWritten == n);
    return totalWritten;
}
