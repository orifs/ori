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

#ifndef __EVBUFSTREAM_H__
#define __EVBUFSTREAM_H__

#include <event2/buffer.h>

#include <oriutil/stream.h>

class evbufstream : public bytestream
{
public:
    evbufstream(struct evbuffer *inbuf);

    bool ended();
    size_t read(uint8_t *buf, size_t n);
    size_t sizeHint() const;

private:
    struct evbuffer *buf;
    size_t bufSize;
};

class evbufwstream : public bytewstream
{
public:
    evbufwstream(struct evbuffer *inbuf = NULL);
    ~evbufwstream();
    ssize_t write(const void *ptr, size_t n);

    struct evbuffer *buf() const;

private:
    struct evbuffer *_buf;
    bool owner;
};

#endif /* __EVBUFSTREAM_H__ */

