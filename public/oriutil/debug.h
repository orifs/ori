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

#define LEVEL_SYS       0 /* Assert/Panic/Abort/NotImplemented */
#define LEVEL_ERR       1 /* Error */
#define LEVEL_MSG       2 /* Stdout */
#define LEVEL_LOG       3 /* Log */
#define LEVEL_DBG       4 /* Debug */
#define LEVEL_VRB       5 /* Verbose */

/*
 * Remove all logging in PERF builds
 */
#ifdef ORI_PERF
#define WARNING(fmt, ...)
#define MSG(fmt, ...)
#define LOG(fmt, ...)
#else
#define WARNING(fmt, ...) ori_log(LEVEL_ERR, "WARNING: " fmt "\n", ##__VA_ARGS__)
#define MSG(fmt, ...) ori_log(LEVEL_MSG, fmt "\n", ##__VA_ARGS__)
#define LOG(fmt, ...) ori_log(LEVEL_LOG, fmt "\n", ##__VA_ARGS__)
#endif

/*
 * Only DEBUG builds compile in DLOG messages
 */
#ifdef DEBUG
#define DLOG(fmt, ...) ori_log(LEVEL_DBG, fmt "\n", ##__VA_ARGS__)
#else
#define DLOG(fmt, ...)
#endif

#ifdef DEBUG
#define ASSERT(_x) \
    if (!(_x)) { \
        ori_log(LEVEL_SYS, "ASSERT("#_x"): %s %s:%d\n", \
                __FUNCTION__, __FILE__, __LINE__); \
        assert(_x); \
    }
#else
#define ASSERT(_x)
#endif

#ifdef _WIN32
#define PANIC() { ori_log(LEVEL_SYS, "PANIC: " \
                         "function %s, file %s, line %d\n", \
                         __FUNCTION__, __FILE__, __LINE__); abort(); }
#define NOT_IMPLEMENTED(_x) if (!(_x)) { ori_log(LEVEL_SYS, \
                                "NOT_IMPLEMENTED(" #_x "): %s %s:%d\n", \
                                __FUNCTION__, __FILE__, __LINE__); abort(); }
#else /* _WIN32 */
#define PANIC() { ori_log(LEVEL_SYS, "PANIC: " \
                         "function %s, file %s, line %d\n", \
                         __func__, __FILE__, __LINE__); abort(); }
#define NOT_IMPLEMENTED(_x) if (!(_x)) { ori_log(LEVEL_SYS, \
                                "NOT_IMPLEMENTED(" #_x "): %s %s:%d\n", \
                                __func__, __FILE__, __LINE__); abort(); }
#endif /* _WIN32 */

#endif /* __DEBUG_H__ */

