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
#include <boost/tr1/memory.hpp>

#include <oriutil/stream.h>
#include "object.h"
#include "packfile.h"

class LocalObject : public Object
{
public:
    typedef std::tr1::shared_ptr<LocalObject> sp;

    LocalObject(PfTransaction::sp transaction, size_t ix);
    LocalObject(Packfile::sp packfile, const IndexEntry &entry);
    ~LocalObject();

    // BaseObject implementation
    bytestream *getPayloadStream();

private:
    PfTransaction::sp transaction;
    size_t ix_tr;

    Packfile::sp packfile;
    IndexEntry entry;
    //void setupLzma(lzma_stream *strm, bool encode);
    //bool appendLzma(int dstFd, lzma_stream *strm, lzma_action action);
};



#endif /* __LOCALOBJECT_H__ */

