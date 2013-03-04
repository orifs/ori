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

#include <iostream>

using namespace std;

int OriUtil_selfTest(void);
int LRUCache_selfTest(void);
int KVSerializer_selfTest(void);
int OriCrypt_selfTest(void);
int Key_selfTest(void);

int
main(int argc, const char *argv[])
{
    int result = 0;
    result += OriUtil_selfTest();
    result += LRUCache_selfTest();
    result += KVSerializer_selfTest();
    result += OriCrypt_selfTest();
    //result += Key_selfTest();

    if (result == 0) {
        cout << "All tests passed!" << endl;
    } else {
        cout << -result << " errors occurred." << endl;
    }

    return 0;
}

