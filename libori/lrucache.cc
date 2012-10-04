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

#include <assert.h>

#include <iostream>

#include <ori/debug.h>
#include <ori/lrucache.h>

using namespace std;

int
LRUCache_selfTest(void)
{
    LRUCache<string, string, 4> cache = LRUCache<string, string, 4>();

    cache.put("A", "1");
    cache.put("B", "2");
    cache.put("C", "3");
    cache.put("D", "4");

    assert(cache.hasKey("A"));
    assert(cache.hasKey("B"));
    assert(cache.hasKey("C"));
    assert(cache.hasKey("D"));

    // Test eviction
    cache.put("E", "5");

    assert(cache.hasKey("E"));
    assert(!cache.hasKey("A"));

    // Test LRU reordering
    cache.get("B");
    cache.put("F", "6");

    assert(cache.hasKey("B"));
    assert(!cache.hasKey("C"));

    // Replacing key's should not evict
    cache.put("B", "NEW");

    assert(cache.hasKey("B"));
    assert(cache.hasKey("D"));
    assert(cache.hasKey("E"));
    assert(cache.hasKey("F"));

    return 0;
}

