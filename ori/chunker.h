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

#ifndef __CHUNKER_H__
#define __CHUNKER_H__

class ChunkerCB
{
public:
    // Report a match
    virtual void match(const uint8_t *buf, uint32_t len) = 0;
    /*
     * Request more data from the buffer. This method should return 1 when 
     * there is more data available and zero when done. Offset should always be 
     * set to point to at least hashLen bytes.
     */
    virtual int load(uint8_t **buf, uint64_t *len, uint64_t *off) = 0;
};

template<int target, int min, int max>
class Chunker
{
public:
    Chunker();
    ~Chunker();
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
Chunker<target, min, max>::Chunker()
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
Chunker<target, min, max>::~Chunker()
{
}

template<int target, int min, int max>
void Chunker<target, min, max>::chunk(ChunkerCB *cb)
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
    for (; off < len - max; off++) {
        for (; off < start + min; off++)
            hash = (hash - lut[in[off-hashLen]]) * b + in[off];

        for (; off < start + max; off++) {
            hash = (hash - lut[in[off-hashLen]]) * b + in[off];
            if (hash % target == 1)
                break;
        }

        cb->match(in + start, off - start);
        start = off + 1;
    }

    if (cb->load(&in, &len, &off) == 1)
	goto fastPath;

    for (; off < len; off++) {
        hash = (hash - lut[in[off-hashLen]]) * b + in[off];
        if (((off - start > min) && (hash % target == 1))
                || (off - start >= max)) {
            cb->match(in + start, off - start);
            start = off + 1;
        }
    }

    if (start < off) {
        cb->match(in + start, off - start);
    }

    return;
}

#endif /* __CHUNKER_H__ */

