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

#define __STDC_LIMIT_MACROS

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include <openssl/sha.h>

#ifdef OPENSSL_NO_SHA256
#error "SHA256 not supported!"
#endif

#include "debug.h"
#include "tuneables.h"
#include "object.h"
#include "localobject.h"

using namespace std;

/*
 * Object
 */
LocalObject::LocalObject(Packfile::sp packfile, const IndexEntry &entry)
    : Object(entry.info), packfile(packfile), entry(entry)
{
}

LocalObject::~LocalObject()
{
}

bytestream *LocalObject::getPayloadStream() {
    return packfile->getPayload(entry);
}

/*
 * Static methods
 */



/*
 * Private methods
 */

// Code from xz_pipe_comp.c in xz examples

/*void LocalObject::setupLzma(lzma_stream *strm, bool encode) {
    lzma_ret ret_xz;
    if (encode) {
        ret_xz = lzma_easy_encoder(strm, 0, LZMA_CHECK_NONE);
    }
    else {
        ret_xz = lzma_stream_decoder(strm, UINT64_MAX, 0);
    }
    assert(ret_xz == LZMA_OK);
}

bool LocalObject::appendLzma(int dstFd, lzma_stream *strm, lzma_action action) {
    uint8_t outbuf[COPYFILE_BUFSZ];
    do {
        strm->next_out = outbuf;
        strm->avail_out = COPYFILE_BUFSZ;

        lzma_ret ret_xz = lzma_code(strm, action);
        if (ret_xz == LZMA_OK || ret_xz == LZMA_STREAM_END) {
            size_t bytes_to_write = COPYFILE_BUFSZ - strm->avail_out;
            int err = write(dstFd, outbuf, bytes_to_write);
            if (err < 0) {
                // TODO: retry write if interrupted?
                return false;
            }
        }
        else {
            ori_log("lzma_code error: %d\n", (int)ret_xz);
            return false;
        }
    } while (strm->avail_out == 0); // i.e. output isn't finished

    return true;
}
*/
