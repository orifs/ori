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
#include <string.h>

#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include <ori/debug.h>
#include <ori/oriutil.h>
#include <ori/objecthash.h>

ObjectHash::ObjectHash()
{
    clear();
}

// Private constructor
ObjectHash::ObjectHash(const char *source)
{
    memcpy(hash, source, SIZE);
}

static uint8_t hexdigit(char c)
{
    ASSERT(isdigit(c) || (c >= 'a' && c <= 'f'));
    if (isdigit(c))
        return c - '0';
    return c - 'a' + 10;
}

ObjectHash ObjectHash::fromHex(std::string hex)
{
    ASSERT(hex.size() == SIZE*2);

    std::transform(hex.begin(), hex.end(), hex.begin(), ::tolower);

    char hash[SIZE];
    for (size_t i = 0; i < SIZE; i++) {
        hash[i] = hexdigit(hex[i*2]) * 16 + hexdigit(hex[i*2+1]);
    }

    ObjectHash rval(hash);
    ASSERT(rval.hex() == hex);
    return rval;
}

/*bool ObjectHash::operator <(const ObjectHash &other) const
{
    return memcmp(hash, other.hash, SIZE) < 0;
}

bool ObjectHash::operator ==(const ObjectHash &other) const
{
    return memcmp(hash, other.hash, SIZE) == 0;
}*/

void ObjectHash::clear()
{
    memset(hash, 0, SIZE);
}

bool ObjectHash::isEmpty() const
{
    for (size_t i = 0; i < SIZE; i++) {
        if (hash[i] != 0)
            return false;
    }
    return true;
}

std::string ObjectHash::hex() const
{
    std::stringstream rval;

    // Convert into string.
    for (size_t i = 0; i < SIZE; i++)
    {
	rval << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return rval.str();
}

std::string ObjectHash::bin() const
{
    return std::string((const char *)hash, SIZE);
}
