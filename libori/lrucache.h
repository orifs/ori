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

#ifndef __LRUCACHE_H__
#define __LRUCACHE_H__

#include <assert.h>

#include "debug.h"

#include <list>
#include <utility>
#include <tr1/unordered_map>


template <class K, class V, int MAX>
class LRUCache
{
public:
    typedef std::list<K> lru_list;
    typedef std::tr1::unordered_map<K,
            std::pair<V, typename lru_list::iterator> > lru_cache;
    LRUCache() {
    }
    ~LRUCache() {
    }
    void put(K key, V value) {
        typename lru_cache::iterator it = cache.find(key);

        if (lru.size() >= MAX && it == cache.end())
            evict();
        if (it != cache.end())
            lru.erase((*it).second.second);

        typename lru_list::iterator p = lru.insert(lru.end(), key);
        cache[key] = std::make_pair(value, p);
    }
    V &get(K key) {
        typename lru_cache::iterator it = cache.find(key);

        if (it != cache.end()) {
            lru.splice(lru.end(), lru, (*it).second.second);
            return (*it).second.first;
        }
        // throw
        //return NULL;
        assert(false);
    }
    bool hasKey(K key) {
        typename lru_cache::iterator it = cache.find(key);
        return it != cache.end();
    }
    void invalidate(K key) {
        typename lru_cache::iterator it = cache.find(key);
        if (it == cache.end()) return;

        lru.erase((*it).second.second);
        cache.erase(it);
    }
    void clear() {
        lru.clear();
        cache.clear();
    }
private:
    void evict()
    {
        typename lru_cache::iterator it = cache.find(lru.front());

        assert(!lru.empty());

        cache.erase(it);
        lru.pop_front();
    }
    lru_list lru;
    lru_cache cache;
};

#endif /* __LRUCACHE_H__ */

