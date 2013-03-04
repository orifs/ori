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

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <iostream>

#include <oriutil/oriutil.h>
#include <ori/localrepo.h>

using namespace std;

extern LocalRepo repository;

int
cmd_catobj(int argc, const char *argv[])
{
    long len;
    const unsigned char *rawBuf;
    string buf;
    bool hex = false;

    ObjectHash id = ObjectHash::fromHex(argv[1]);

    len = repository.getObjectLength(id);
    if (len == -1) {
	printf("Object does not exist.\n");
	return 1;
    }

    buf = repository.getPayload(id);
    rawBuf = (const unsigned char *)buf.data();

    for (int i = 0; i < len; i++) {
        if (rawBuf[i] < 1 || rawBuf[i] >= 0x80) {
            hex = true;
            break;
        }
    }

    if (hex) {
        printf("Hex Dump (%ld bytes):\n", len);
        Util_PrintHex(buf, 0, len);
        printf("\n");
    } else {
        printf("%s", rawBuf);
    }

    return 0;
}

