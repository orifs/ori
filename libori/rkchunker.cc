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
#include <stdio.h>
#include <stdlib.h>

#include <time.h>
#include <sys/time.h>

#include <openssl/sha.h>

#include <string>
#include <iostream>

#include "rkchunker.h"

class TestCB : public ChunkerCB
{
public:
    TestCB(uint64_t testLen)
    {
	chunks = 0;
	chunkLen = 0;
	buf = new uint8_t[testLen];
	len = testLen;
    }
    ~TestCB()
    {
	delete buf;
	buf = NULL;
    }
    void fill()
    {
#ifdef __FreeBSD__
	sranddev();
#endif /* __FreeBSD__ */

	for (uint64_t i = 0; i < len; i++)
	{
	    buf[i] = rand() % 256;
	}
    }
    virtual void match(const uint8_t *b, uint32_t l)
    {
	SHA256_CTX state;
	unsigned char hash[SHA256_DIGEST_LENGTH];

	chunks++;
	chunkLen += l;

	SHA256_Init(&state);
	SHA256_Update(&state, b, l);
	SHA256_Final(hash, &state);
    }
    virtual int load(uint8_t **b, uint64_t *l, uint64_t *o)
    {
	if (len != 0) {
	    *b = buf;
	    *l = len;
	    *o = 0;
	    len = 0;
	    return 1;
	} else {
	    return 0;
	}
    }
    uint64_t chunks;
    uint64_t chunkLen;
    uint8_t *buf;
    uint64_t len;
};

#define TEST_LEN (1024 * 1024 * 1024)

int main(int argc, char *argv[])
{
    TestCB cb = TestCB(TEST_LEN);
    RKChunker<4096, 2048, 8192> c = RKChunker<4096, 2048, 8192>();

    cb.fill();

    struct timeval start, end;
    gettimeofday(&start, 0);
    c.chunk(&cb);
    gettimeofday(&end, 0);

    float tDiff = end.tv_sec - start.tv_sec;
    tDiff += (float)(end.tv_usec - start.tv_usec) / 10000000.0;

    printf("Chunks %llu, Avg Chunk %llu\n", cb.chunks, cb.chunkLen / cb.chunks);
    printf("Time %3.3f, Speed %3.2fMB/s\n", tDiff, 1024.0 / tDiff);
}

