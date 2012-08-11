#ifndef __STREAM_H__
#define __STREAM_H__

#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

#include <lzma.h>

#include "objecthash.h"
#include "tuneables.h"

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
    size_t readPStr(std::string &str);

    /// Read ObjectHash
    void readHash(ObjectHash &out);

    /// Reads an integer with proper byte-swapping (TODO)
    template <typename T>
    T readInt() {
        T rval;
        bool success = readExact((uint8_t*)&rval, sizeof(T));
        assert(success);
        //std::cerr << "Read int " << (ssize_t)rval << std::endl;
        return rval;
    }
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

class lzmastream : public bytestream
{
public:
    /// Takes ownership of source. size_hint is total number of bytes output (from read) 
    lzmastream(bytestream *source, bool compress = false, size_t size_hint = 0);
    ~lzmastream();
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
    void writePStr(const std::string &str);
    void writeHash(const ObjectHash &hash);

    /// Writes an integer with proper byte-swapping (TODO)
    template <typename T>
    void writeInt(const T &n) {
        //std::cerr << "Wrote int " << (ssize_t)n << std::endl;
        write(&n, sizeof(T));
    }
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

