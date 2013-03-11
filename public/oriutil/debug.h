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

#if defined(_WIN32)
void ori_log(int level, const char *fmt, ...);
#else
void ori_log(int level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
#endif
int ori_open_log(const std::string &logPath);

#define LEVEL_ERR	0 /* Error */
#define LEVEL_MSG	1 /* Stdout */
#define LEVEL_LOG	2 /* Log */
#define LEVEL_DBG	3 /* Debug */
#define LEVEL_VRB	4 /* Verbose */

#ifdef DEBUG
#define WARNING(fmt, ...) ori_log(LEVEL_ERR, "WARNING: " fmt "\n", ##__VA_ARGS__)
#define MSG(fmt, ...) ori_log(LEVEL_MSG, fmt "\n", ##__VA_ARGS__)
#define LOG(fmt, ...) ori_log(LEVEL_LOG, fmt "\n", ##__VA_ARGS__)
#else
#define WARNING(fmt, ...)
#define MSG(fmt, ...)
#define LOG(fmt, ...)
#endif

#ifdef DEBUG
#define ASSERT(_x) assert(_x)
#else
#define ASSERT(_x)
#endif

#ifdef _WIN32
#define PANIC() { printf("PANIC: " \
                         "function %s, file %s, line %d\n", \
                         __FUNCTION__, __FILE__, __LINE__); abort(); }
#define NOT_IMPLEMENTED(_x) if (!_x) { printf("NOT_IMPLEMENTED: (" #_x "), " \
                                "function %s, file %s, line %d\n", \
                                __FUNCTION__, __FILE__, __LINE__); abort(); }
#else /* _WIN32 */
#define PANIC() { printf("PANIC: " \
                         "function %s, file %s, line %d\n", \
                         __func__, __FILE__, __LINE__); abort(); }
#define NOT_IMPLEMENTED(_x) if (!_x) { printf("NOT_IMPLEMENTED: (" #_x "), " \
                                "function %s, file %s, line %d\n", \
                                __func__, __FILE__, __LINE__); abort(); }
#endif /* _WIN32 */

#endif /* __DEBUG_H__ */

