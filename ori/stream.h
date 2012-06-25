#ifndef __STREAM_H__
#define __STREAM_H__

#include <vector>

#include <lzma.h>

class bytestream
{
public:
    bytestream() {}
    virtual ~bytestream() {};

    virtual bool ended() = 0;
    virtual size_t read(uint8_t *buf, size_t n) = 0;
    //virtual const char *error() = 0;
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

class lzmastream : public bytestream
{
public:
    lzmastream(bytestream *source);
    bool ended();
    size_t read(uint8_t *, size_t);

private:
    bytestream *source;
    bool output_ended;
    lzma_stream strm;
    std::vector<uint8_t> buffer;

    bool _readMore();
};

#endif

