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

#include <stdexcept>

#include <ori/debug.h>
#include "evbufstream.h"

evbufstream::evbufstream(struct evbuffer *inbuf)
    : buf(inbuf), bufSize(0)
{
    ASSERT(inbuf != NULL);
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
