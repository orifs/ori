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

#ifndef __LOCALOBJECT_H__
#define __LOCALOBJECT_H__

#include <stdint.h>

#include <utility>
#include <string>
#include <map>
#include <tr1/memory>

#include "stream.h"
#include "object.h"

class LocalObject : public Object
{
public:
    typedef std::tr1::shared_ptr<LocalObject> sp;

    LocalObject();
    ~LocalObject();
    int create(const std::string &path, Type type, uint32_t flags = ORI_FLAG_DEFAULT);
    int createFromRawData(const std::string &path, const ObjectInfo &info,
            const std::string &raw_data);
    int open(const std::string &path, const std::string &hash="");
    void close();
    size_t getFileSize();

    // BaseObject implementation
    std::auto_ptr<bytestream> getPayloadStream();
    std::auto_ptr<bytestream> getStoredPayloadStream();
    size_t getStoredPayloadSize();

    // Payload Operations
    int purge();
    int setPayload(bytestream *bs);
    int setPayload(const std::string &blob);
    std::string computeHash();

private:
    int fd;
    size_t storedLen;
    std::string objPath;
    size_t fileSize;

    void setupLzma(lzma_stream *strm, bool encode);
    bool appendLzma(int dstFd, lzma_stream *strm, lzma_action action);
};



#endif /* __LOCALOBJECT_H__ */

