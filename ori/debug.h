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

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void ori_log(const char *fmt, ...);

#ifdef DEBUG
#define LOG(fmt, ...) ori_log(fmt "\n", ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

#define ASSERT(_x) assert(_x)
#define NOT_IMPLEMENTED(_x) if (!_x) { printf("NOT_IMPLEMENTED: (" #_x "), " \
                                "function %s, file %s, line %d\n", \
                                __func__, __FILE__, __LINE__); abort(); }

#endif /* __DEBUG_H__ */

