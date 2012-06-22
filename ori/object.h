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

#ifndef __OBJECT_H__
#define __OBJECT_H__

#include <stdint.h>

#include <utility>
#include <string>
#include <map>

#ifndef ORI_USE_COMPRESSION
#define ORI_USE_COMPRESSION 1
#endif

#if ORI_USE_COMPRESSION
#include <lzma.h>
#endif


#define ORI_OBJECT_TYPESIZE	    4
#define ORI_OBJECT_FLAGSSIZE	4
#define ORI_OBJECT_SIZE		    8
#define ORI_OBJECT_HDRSIZE	    24

#define ORI_FLAG_COMPRESSED 0x0001

#define ORI_FLAG_DEFAULT ORI_FLAG_COMPRESSED

class Object
{
public:
    enum Type { Null, Commit, Tree, Blob, LargeBlob, Purged };
    enum BRState { BRNull, BRRef, BRPurged };
    Object();
    ~Object();
    int create(const std::string &path, Type type, uint32_t flags = ORI_FLAG_DEFAULT);
    int open(const std::string &path);
    void close();
    Type getType();
    size_t getDiskSize();
    size_t getObjectSize();
    size_t getObjectStoredSize();
    // Flags operations (TODO)
    void checkFlags();
    bool getCompressed();
    // Payload Operations
    int purge();
    int appendFile(const std::string &path);
    int extractFile(const std::string &path);
    int appendBlob(const std::string &blob);
    std::string extractBlob();
    std::string computeHash();
    // Backreferences
    void addBackref(const std::string &objId, BRState state);
    void updateBackref(const std::string &objId, BRState state);
    void clearBackref();
    std::map<std::string, BRState> getBackref();
private:
    int fd;
    int flags;
    Type t;
    int64_t len;
    int64_t storedLen;
    std::string objPath;

#if ORI_USE_COMPRESSION
    void setupLzma(lzma_stream *strm);
    bool appendLzma(lzma_stream *strm, lzma_action action);
#endif
};

#endif /* __OBJECT_H__ */

