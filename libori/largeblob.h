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

#ifndef __LARGEBLOB_H__
#define __LARGEBLOB_H__

#include <stdint.h>

#include <string>
#include <map>

#include "localrepo.h"

class LBlobEntry
{
public:
    LBlobEntry(const LBlobEntry &l);
    LBlobEntry(const std::string &hash, uint16_t length);
    ~LBlobEntry();
    const std::string hash;
    const uint16_t length;
};

class Repo;

class LargeBlob
{
public:
    LargeBlob(LocalRepo *r);
    ~LargeBlob();
    void chunkFile(const std::string &path);
    void extractFile(const std::string &path);
    /// May read less than s bytes
    ssize_t read(uint8_t *buf, size_t s, off_t off);
    // XXX: Stream read/write operations
    const std::string getBlob();
    void fromBlob(const std::string &blob);
    size_t totalSize() const;
    /*
     * A map of the file parts contains the file offset as the key and the large 
     * blob entry object as the value.  It allows O(log n) random access into 
     * the file.
     */
    std::string hash;
    std::map<uint64_t, LBlobEntry> parts;
    LocalRepo *repo;
};

#endif /* __LARGEBLOB_H__ */

