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

#ifndef __OBJECTHASH_H__
#define __OBJECTHASH_H__

#include <stdint.h>
#include <string.h>

struct ObjectHash {
    static const size_t SIZE = 32;
    static const size_t STR_SIZE = SIZE * 2;

    ObjectHash();
    static ObjectHash fromHex(std::string hex);

    bool operator <(const ObjectHash &other) const {
        return memcmp(hash, other.hash, SIZE) < 0;
    }
    bool operator ==(const ObjectHash &other) const {
        return memcmp(hash, other.hash, SIZE) == 0;
    }
    bool operator !=(const ObjectHash &other) const {
        return !(*this == other);
    }

    void clear();
    // TODO: empty hash "\0...\0" is valid?...
    bool isEmpty() const;
    /// Returns the hash as a hex string (size 2*SIZE)
    std::string hex() const;
    /// Copies the binary hash to a string (length SIZE)
    std::string bin() const;

    uint8_t hash[SIZE];
    
private:
    ObjectHash(const char *source);
};

std::size_t hash_value(ObjectHash const& key);

#endif /* __OBJECTHASH_H__ */

