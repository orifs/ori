#include <stdexcept>

#include "evbufstream.h"
#include "debug.h"


evbufstream::evbufstream(struct evbuffer *inbuf)
    : buf(inbuf), bufSize(0)
{
    assert(inbuf != NULL);
    bufSize = evbuffer_get_length(buf);
}

bool evbufstream::ended()
{
    return evbuffer_get_length(buf) == 0;
}

size_t evbufstream::read(uint8_t *buf, size_t n)
{
    int status = evbuffer_remove(this->buf, buf, n);
    if (status < 0) return 0;
    return status;
}

size_t evbufstream::sizeHint() const
{
    return bufSize;
}



evbufwstream::evbufwstream(struct evbuffer *inbuf)
    : _buf(inbuf), owner(false)
{
    if (inbuf == NULL) {
        _buf = evbuffer_new();
        if (_buf == NULL) {
            LOG("evbufwstream: evbuffer_new failed!");
            throw std::runtime_error("evbuffer_new failed");
        }
        owner = true;
    }
}

evbufwstream::~evbufwstream()
{
    if (_buf != NULL && owner) {
        evbuffer_free(_buf);
    }
}

ssize_t evbufwstream::write(const void *ptr, size_t n)
{
    if (evbuffer_add(_buf, ptr, n) == 0)
        return n;
    return -1;
}

struct evbuffer *evbufwstream::buf() const
{
    return _buf;
}
