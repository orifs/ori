/*
 * Copyright (c) 2012-2014 Stanford University
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

#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <openssl/sha.h>

#include <string>
#include <iostream>

#include <oriutil/debug.h>
#include <oriutil/orifile.h>
#include <oriutil/oricrypt.h>

#include "rkchunker.h"

using namespace std;

#define MIN(X, Y) (X < Y ? X : Y)

class FileChunkerCB : public ChunkerCB
{
public:
    FileChunkerCB()
    {
        lbOff = 0;
        buf = NULL;
    }
    ~FileChunkerCB()
    {
        if (buf)
            delete[] buf;
        if (srcFd > 0)
            ::close(srcFd);
    }
    int open(const string &path)
    {
        struct stat sb;

        bufLen = 8 * 1024 * 1024;
        buf = new uint8_t[bufLen];
        if (buf == NULL)
            return -ENOMEM;

        srcFd = ::open(path.c_str(), O_RDONLY);
        if (srcFd < 0)
            return -errno;

        if (fstat(srcFd, &sb) < 0) {
            close(srcFd);
            return -errno;
        }
        fileLen = sb.st_size;
        fileOff = 0;

        return 0;
    }
    virtual void match(const uint8_t *b, uint32_t l)
    {
        // Add the fragment into the repository
        // XXX: Journal for cleanup!
        string blob = string((const char *)b, l);
        ObjectHash hash = OriCrypt_HashString(blob);
        //lb->repo->addObject(ObjectInfo::Blob, hash, blob);

        // Add the fragment to the LargeBlob object.
        //lb->parts.insert(make_pair(lbOff, LBlobEntry(hash, l)));
        printf("%08llx %s\n", lbOff, hash.hex().c_str());
        lbOff += l;
    }
    virtual int load(uint8_t **b, uint64_t *l, uint64_t *o)
    {
        printf("buf = %p, l = %08llxx, o = %08llx\n", *b,*l, *o);

        if (*b == NULL)
            *b = buf;

        if (fileOff == fileLen)
            return 0;

        // Sanity checking
        ASSERT(*b == buf);
        ASSERT(*l <= bufLen);
        ASSERT(*o <= bufLen);

        if (*o != 0 || *o != *l) {
            ASSERT(*o > 32); // XXX: Must equal hashLen
            ASSERT(*l > 32);
            memcpy(buf, buf + *o - 32, *l - *o + 32);
            *l = *l - *o + 32;
            *o = 32;
        }

        uint64_t toRead = MIN(bufLen - *l, fileLen - fileOff);
        int status;

        status = ::read(srcFd, buf + *l, toRead);
        if (status < 0) {
            printf("buf = %p, l = %08llx, toRead = %08llx\n", buf, *l, toRead);
            perror("Cannot read large file");
            PANIC();
            return -1;
        }
        ASSERT(status == (int)toRead);

        fileOff += status;
        *l += status;
        //*o = 32;

        return 1;
    }
private:
    // Output large blob
    uint64_t lbOff;
    // Input file
    int srcFd;
    uint64_t fileLen;
    uint64_t fileOff;
    // RK buffer
    uint8_t *buf;
    uint64_t bufLen;
};

void
chunkFile(const string &path)
{
    int status;
    FileChunkerCB cb = FileChunkerCB();
#ifdef ORI_USE_RK
    RKChunker<4096, 2048, 8192> c = RKChunker<4096, 2048, 8192>();
#endif /* ORI_USE_RK */

#ifdef ORI_USE_FIXED
    //FChunker<4096> c = FChunker<4096>();
    FChunker<32*1024> c = FChunker<32*1024>();
#endif /* ORI_USE_FIXED */

    status = cb.open(path);
    if (status < 0) {
        perror("Cannot open large file for chunking");
        PANIC();
        return;
    }

    //totalHash = OriCrypt_HashFile(path);

    c.chunk(&cb);
}

int main(int argc, char *argv[])
{
    string filePath;
    struct timeval start, end;
    gettimeofday(&start, 0);

    if (argc != 2) {
        printf("rkchunker_test requires a filename to chunk!\n");
        return 1;
    }
    filePath = argv[1];
    chunkFile(filePath);

    gettimeofday(&end, 0);

    float tDiff = end.tv_sec - start.tv_sec;
    tDiff += (float)(end.tv_usec - start.tv_usec) / OriFile_GetSize(filePath);

    //printf("Chunks %llu, Avg Chunk %llu\n", cb.chunks, cb.chunkLen / cb.chunks);
    printf("Time %3.3f, Speed %3.2fMB/s\n", tDiff, OriFile_GetSize(filePath) / (1024.0 * 1024.0) / tDiff);
}

