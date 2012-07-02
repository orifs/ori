#ifndef __STREAM_H__
#define __STREAM_H__

#include <vector>
#include <string>

#include <lzma.h>

class bytestream
{
public:
    typedef std::auto_ptr<bytestream> ap;

    bytestream() : last_errnum(0) {}
    virtual ~bytestream() {};

    virtual bool ended() = 0;
    virtual size_t read(uint8_t *buf, size_t n) = 0;
    virtual size_t sizeHint() const = 0;

    virtual const char *error() { return last_error.size() > 0 ? last_error.c_str() : NULL; }
    virtual int errnum() { return last_errnum; }

    // Stream utils
    std::string readAll();
    int copyToFile(const std::string &path);
    int copyToFd(int dstFd);
protected:
    std::string last_error;
    int last_errnum;

    void setErrno(const char *msg);
    bool inheritError(bytestream::ap bs);
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
    lzmastream(bytestream *source, size_t size_hint = 0);
    ~lzmastream();
    bool ended();
    size_t read(uint8_t *, size_t);
    size_t sizeHint() const;

private:
    bytestream::ap source;
    size_t size_hint;

    bool output_ended;
    lzma_stream strm;
    uint8_t in_buf[XZ_READ_BY];

    void setLzmaErr(const char *msg, lzma_ret ret);
};

#endif

