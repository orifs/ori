#ifndef __STREAM_H__
#define __STREAM_H__

#include <cassert>
#include <vector>
#include <string>
#include <memory>

#include <lzma.h>

#include "objecthash.h"

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

    // Stream utils
    std::string readAll();
    int copyToFile(const std::string &path);
    int copyToFd(int dstFd);

    /// Read Pascal-style string (1 byte length)
    void readPStr(std::string &str);

    /// Read ObjectHash
    void readHash(ObjectHash &out);

    /// Reads an integer with proper byte-swapping (TODO)
    template <typename T>
    T readInt() {
        T rval;
        size_t nbytes = read((uint8_t*)&rval, sizeof(T));
        assert(nbytes == sizeof(T));
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
    const std::string &buf;
    size_t off;
    size_t len;
};

class fdstream : public bytestream
{
public:
    fdstream(int fd, off_t offset, size_t length);
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


#define XZ_READ_BY 4096

class lzmastream : public bytestream
{
public:
    /// Takes ownership of source. size_hint is number of bytes that can be read
    lzmastream(bytestream *source, bool compress = false, size_t size_hint = 0);
    ~lzmastream();
    bool ended();
    size_t read(uint8_t *, size_t);
    size_t sizeHint() const;

private:
    bytestream *source;
    size_t size_hint;

    bool output_ended;
    lzma_stream strm;
    uint8_t in_buf[XZ_READ_BY];

    void setLzmaErr(const char *msg, lzma_ret ret);
};



////////////////////////////////
// Writable streams

class bytewstream : public basestream
{
public:
    bytewstream() {}
    virtual ~bytewstream() {}

    virtual void write(const void *, size_t) = 0;

    // High-level functions
    void copyFrom(bytestream *bs);
    void writePStr(const std::string &str);
    void writeHash(const ObjectHash &hash);

    /// Writes an integer with proper byte-swapping (TODO)
    template <typename T>
    void writeInt(const T &n) {
        write(&n, sizeof(T));
    }
};



class strwstream : public bytewstream
{
public:
    strwstream();
    strwstream(const std::string &);
    strwstream(size_t reserved);
    void write(const void *, size_t);

    const std::string &str() const;
private:
    std::string buf;
};

#endif

