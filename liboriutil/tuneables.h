/*
 * Copyright (c) 2012-2013 Stanford University
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

#ifndef __TUNEABLES_H__
#define __TUNEABLES_H__

#define COPYFILE_BUFSZ	(256 * 1024)
#define HASHFILE_BUFSZ	(256 * 1024)
#define COMPFILE_BUFSZ  (16 * 1024)

// Choose the hash algorithm (choose one)
//#define ORI_USE_SHA256
//#define ORI_USE_SKEIN
#if !defined(ORI_USE_SHA256) && !defined(ORI_USE_SKEIN)
#error "Please select one hash algorithm."
#endif

// Choose the compression algorithm (choose one)
//#define ORI_USE_LZMA
//#define ORI_USE_FASTLZ
#if defined(ORI_USE_COMPRESISON) && !defined(ORI_USE_LZMA) && !defined(ORI_USE_FASTLZ)
#error "Please select one compression algorithm."
#endif

#endif /* __TUNEABLES_H__ */

