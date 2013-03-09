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

#ifndef __STREAM_H__
#define __STREAM_H__

#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

#ifdef ORI_USE_LZMA
#include <lzma.h>
#endif /* ORI_USE_LZMA */

#include "oriutil.h"
#include "objecthash.h"
#include "objectinfo.h"

class basestream
{
public:
    basestream() : last_errnum(0) {}

    virtual const char *error() { return last_error.size() > 0 ? last_error.c_str() : NULL; }
    virtual int errnum() { return last_errnum; }

protected:
    std::string last_error;
    int last_errnum;

    void setErrno(const char *msg);
    bool inheritError(basestream *bs);
};

class bytestream : public basestream
{
public:
    typedef std::auto_ptr<bytestream> ap;

    bytestream() {}
    virtual ~bytestream() {};

    virtual bool ended() = 0;
    virtual size_t read(uint8_t *buf, size_t n) = 0;
    virtual size_t sizeHint() const = 0;

    bool readExact(uint8_t *buf, size_t n);

    // Stream utils
    std::string readAll();
    int copyToFile(const std::string &path);
    int copyToFd(int dstFd);

    /// Read Pascal-style string (1 byte length)
    int readPStr(std::string &str);
    /// Read Pascal-style string (2 byte length)
    int readLPStr(std::string &str);

    /// Read ObjectHash
    void readHash(ObjectHash &out);
    void readInfo(ObjectInfo &out);

    /// Reads an integer with proper byte-swapping (TODO)
    int8_t readInt8();
    int16_t readInt16();
    int32_t readInt32();
    int64_t readInt64();
    uint8_t readUInt8();
    uint16_t readUInt16();
    uint32_t readUInt32();
    uint64_t readUInt64();
};

class strstream : public bytestream
{
public:
    strstream(const std::string &, size_t start=0);
    bool ended();
    size_t read(uint8_t *, size_t);
    size_t sizeHint() const;
private:
    std::string buf;
    size_t off;
    size_t len;
};

class fdstream : public bytestream
{
public:
    /// @param offset can be -1 to disable seeking
    fdstream(int fd, off_t offset, size_t length=(size_t)-1);
    bool ended();
    size_t read(uint8_t *, size_t);
    size_t sizeHint() const;

private:
    int fd;
    off_t offset;
    size_t length;
    size_t left;
};

class diskstream : public bytestream
{
public:
    diskstream(const std::string &filename);
    ~diskstream();
    bool ended();
    size_t read(uint8_t *, size_t);
    size_t sizeHint() const;

private:
    int fd;
    bytestream::ap source;
};


#define COMPRESS true
#define DECOMPRESS false

#ifdef ORI_USE_LZMA

class zipstream : public bytestream
{
public:
    /// Takes ownership of source. size_hint is total number of bytes output (from read) 
    zipstream(bytestream *source, bool compress = false, size_t size_hint = 0);
    ~zipstream();
    bool ended();
    size_t read(uint8_t *, size_t);
    size_t sizeHint() const;
    size_t inputConsumed() const;

private:
    bytestream *source;
    size_t size_hint;

    bool output_ended;
    lzma_stream strm;
    uint8_t in_buf[COMPFILE_BUFSZ];

    void setLzmaErr(const char *msg, lzma_ret ret);
};

#endif /* ORI_USE_LZMA */

#ifdef ORI_USE_FASTLZ

class zipstream : public bytestream
{
public:
    /// Takes ownership of source. size_hint is total number of bytes output (from read) 
    zipstream(bytestream *source, bool compress = false, size_t size_hint = 0);
    ~zipstream();
    bool ended();
    size_t read(uint8_t *, size_t);
    size_t sizeHint() const;
    size_t inputConsumed() const;

private:
    bytestream *source;
    size_t size_hint;

    bool compress;
    bool input_processed;
    std::string input; // TODO
    std::vector<uint8_t> output;

    size_t offset;
    bool output_ended;
};

#endif /* ORI_USE_FASTLZ */

////////////////////////////////
// Writable streams

class bytewstream : public basestream
{
public:
    typedef std::auto_ptr<bytewstream> ap;

    bytewstream() {}
    virtual ~bytewstream() {}

    virtual ssize_t write(const void *, size_t) = 0;

    // High-level functions
    void copyFrom(bytestream *bs);
    int writePStr(const std::string &str);
    int writeLPStr(const std::string &str);
    void writeHash(const ObjectHash &hash);
    int writeInfo(const ObjectInfo &info);

    /// Writes an integer with proper byte-swapping (TODO)
    int writeInt8(int8_t n);
    int writeInt16(int16_t n);
    int writeInt32(int32_t n);
    int writeInt64(int64_t n);
    int writeUInt8(uint8_t n);
    int writeUInt16(uint16_t n);
    int writeUInt32(uint32_t n);
    int writeUInt64(uint64_t n);
};



class strwstream : public bytewstream
{
public:
    strwstream();
    strwstream(const std::string &);
    strwstream(size_t reserved);
    ssize_t write(const void *, size_t);

    const std::string &str() const;
private:
    std::string buf;
};

class fdwstream : public bytewstream
{
public:
    fdwstream(int fd);
    ssize_t write(const void *, size_t);
private:
    int fd;
};

#endif

