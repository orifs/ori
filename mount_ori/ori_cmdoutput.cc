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

#include "ori_fuse.h"

#include <stdarg.h>
#include <stdlib.h>

#define OUTPUT_MAX 1024

void ori_priv::printf(const char *fmt, ...)
{
    RWKey::sp key = lock_cmd_output.writeLock();

    va_list args;
    va_start(args, fmt);

    char buffer[OUTPUT_MAX];
    vsnprintf(buffer, OUTPUT_MAX, fmt, args);

    outputBuffer += buffer;
    ::printf("Output: %s\n", buffer);

    va_end(args);
}

size_t ori_priv::readOutput(char *buf, size_t n)
{
    if (outputBuffer.size() == 0)
        return 0;

    size_t to_read = n;
    if (to_read > outputBuffer.size())
        to_read = outputBuffer.size();

    memcpy(buf, &outputBuffer[0], to_read);

    size_t newSize = outputBuffer.size() - to_read;
    memmove(&outputBuffer[0], &outputBuffer[to_read], newSize);
    outputBuffer.resize(newSize);
    return to_read;
}
