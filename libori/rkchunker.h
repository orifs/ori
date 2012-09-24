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

#ifndef __RKCHUNKER_H__
#define __RKCHUNKER_H__

#include "chunker.h"

template<int target, int min, int max>
class RKChunker
{
public:
    RKChunker();
    ~RKChunker();
    void chunk(ChunkerCB *cb);
private:
    //uint64_t hashLen;
    uint64_t b;
    uint64_t bTok;
    uint8_t lut[256];
};

//#define b (31)
//#define bTok (3671467063254694913L)
#define hashLen (32)

template<int target, int min, int max>
RKChunker<target, min, max>::RKChunker()
{
    uint64_t bTon = 1;

    //hashLen = 32;
    b = 31;

    for (int i = 0; i < hashLen; i++) {
        bTon *= b;
    }

    bTok = bTon;

    for (int i = 0; i < 256; i++) {
        lut[i] = i * bTok;
    }
}

template<int target, int min, int max>
RKChunker<target, min, max>::~RKChunker()
{
}

template<int target, int min, int max>
void RKChunker<target, min, max>::chunk(ChunkerCB *cb)
{
    uint8_t *in = NULL;
    uint64_t len = 0;
    register uint64_t hash = 0;
    register uint64_t off = 0;
    register uint64_t start = 0;

    if (cb->load(&in, &len, &off) == 0) {
	assert(false);
	return;
    }

    for (off = 0; off < hashLen; off++) {
        hash = hash * b + in[off];
    }

fastPath:
    /*
     * Fast-path avoiding the length tests.off
     */
    for (; off < len - max;) {
        for (; off < start + min && off < len; off++)
            hash = (hash - lut[in[off-hashLen]]) * b + in[off];

        for (; off < start + max && off < len; off++) {
            hash = (hash - lut[in[off-hashLen]]) * b + in[off];
            if (hash % target == 1)
                break;
        }

        cb->match(in + start, off - start);
        start = off;
    }

    if (cb->load(&in, &len, &off) == 1) {
        start = off;
	goto fastPath;
    }

    for (; off < len; off++) {
        hash = (hash - lut[in[off-hashLen]]) * b + in[off];
        if (((off - start > min) && (hash % target == 1))
                || (off - start >= max)) {
            cb->match(in + start, off - start);
            start = off;
        }
    }

    if (start < off) {
        cb->match(in + start, off - start);
    }

    return;
}

#endif /* __RKCHUNKER_H__ */

