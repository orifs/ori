#ifndef __STREAM_H__
#define __STREAM_H__

#include <vector>

#include <lzma.h>

class bytestream
{
public:
    bytestream() : last_error(NULL) {}
    virtual ~bytestream() {};

    virtual bool ended() = 0;
    virtual size_t read(uint8_t *buf, size_t n) = 0;
    virtual const char *error() { return last_error; };
protected:
    const char *last_error;
};

class diskstream : public bytestream
{
public:
    diskstream(int fd, off_t offset, size_t length);
    bool ended();
    size_t read(uint8_t *, size_t);

private:
    int fd;
    off_t offset;
    size_t length;
    size_t left;
};

#define XZ_READ_BY 4096

class lzmastream : public bytestream
{
public:
    lzmastream(bytestream *source);
    ~lzmastream();
    bool ended();
    size_t read(uint8_t *, size_t);

private:
    bytestream *source;
    bool output_ended;
    lzma_stream strm;
    uint8_t in_buf[XZ_READ_BY];
};

#endif

