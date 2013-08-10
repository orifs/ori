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

/*
 * Implements a fixed chunker
 */

#ifndef __FCHUNKER_H__
#define __FCHUNKER_H__

#include "chunker.h"

template<int chunk_size>
class FChunker
{
public:
    FChunker();
    ~FChunker();
    void chunk(ChunkerCB *cb);
};

template<int chunk_size>
FChunker<chunk_size>::FChunker()
{
}

template<int chunk_size>
FChunker<chunk_size>::~FChunker()
{
}

template<int chunk_size>
void FChunker<chunk_size>::chunk(ChunkerCB *cb)
{
    uint8_t *in = NULL;
    uint64_t len = 0;
    register uint64_t off = 0;
    register uint64_t start = 0;

    if (cb->load(&in, &len, &off) == 0) {
        assert(false);
        return;
    }

fastPath:
    while (len > off + chunk_size) {
        off += chunk_size;
        cb->match(in + start, off - start);
        start = off;
    }

    if (cb->load(&in, &len, &off) == 1) {
        start = off;
        goto fastPath;
    }

    if (len > start) {
        cb->match(in + start, len - start);
    }

    return;
}

#endif /* __FCHUNKER_H__ */

