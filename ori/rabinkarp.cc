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

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <time.h>
#include <sys/time.h>

#include <openssl/sha.h>

#include <string>
#include <iostream>

#include "rabinkarp.h"

//#define b (31)
//#define bTok (3671467063254694913L)
#define hashLen (32)

template<int target, int min, int max>
Chunker<target, min, max>::Chunker()
{
    uint64_t bTon = 1;
    this->cb = NULL;

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
void Chunker<target, min, max>::setCB(ChunkerCB *cb)
{
    this->cb = cb;
}

template<int target, int min, int max>
void Chunker<target, min, max>::chunk(char *buf, uint64_t len)
{
    uint8_t *in = (uint8_t *)buf;
    register uint64_t hash = 0;
    register uint64_t off = 0;
    register uint64_t start = 0;

    for (off = 0; off < hashLen; off++) {
        hash = hash * b + in[off];
    }

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

#ifdef RK_TEST

class TestCB : public ChunkerCB
{
public:
    virtual void match(const uint8_t *buf, uint32_t len)
    {
        SHA256_CTX state;
        unsigned char hash[SHA256_DIGEST_LENGTH];

        chunks++;
        chunkLen += len;

        SHA256_Init(&state);
        SHA256_Update(&state, buf, len);
        SHA256_Final(hash, &state);
    }
    uint64_t chunks;
    uint64_t chunkLen;
};

#define TEST_LEN (1024 * 1024 * 1024)

int main(int argc, char *argv[])
{
    TestCB cb = TestCB();
    Chunker<4096, 2048, 8192> c = Chunker<4096, 2048, 8192>();
    char *buf = new char[TEST_LEN];

    sranddev();

    for (int i = 0; i < TEST_LEN; i++)
    {
        buf[i] = rand() % 256;
    }

    struct timeval start, end;
    gettimeofday(&start, 0);
    c.setCB(&cb);
    c.chunk(buf, TEST_LEN);
    gettimeofday(&end, 0);

    float tDiff = end.tv_sec - start.tv_sec;
    tDiff += (float)(end.tv_usec - start.tv_usec) / 10000000.0;

    printf("Chunks %ld, Avg Chunk %ld\n", cb.chunks, cb.chunkLen / cb.chunks);
    printf("Time %3.3f, Speed %3.2fMB/s\n", tDiff, 1024.0 / tDiff);
}

#endif /* RK_TEST */

