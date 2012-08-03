#include <cassert>
#include <stdint.h>
#include <string.h>

#include <string>
#include <algorithm>

#include "objecthash.h"
#include "util.h"

ObjectHash::ObjectHash()
{
    memset(hash, 0, SIZE);
}

// Private constructor
ObjectHash::ObjectHash(const char *source)
{
    memcpy(hash, source, SIZE);
}

uint8_t hexdigit(char c)
{
    assert(isdigit(c) || (c >= 'a' && c <= 'f'));
    if (isdigit(c))
        return c - '0';
    return c - 'a' + 10;
}

ObjectHash ObjectHash::fromHex(std::string hex)
{
    assert(hex.size() == SIZE*2);

    std::transform(hex.begin(), hex.end(), hex.begin(), ::tolower);

    char hash[SIZE];
    for (size_t i = 0; i < SIZE; i++) {
        hash[i] = hexdigit(hex[i*2]) * 16 + hexdigit(hex[i*2+1]);
    }

    ObjectHash rval(hash);
    assert(rval.hex() == hex);
    return rval;
}

bool ObjectHash::operator <(const ObjectHash &other) const
{
    return memcmp(hash, other.hash, SIZE) < 0;
}

bool ObjectHash::operator ==(const ObjectHash &other) const
{
    return memcmp(hash, other.hash, SIZE) == 0;
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
    return Util_RawHashToHex(*this);
}

std::string ObjectHash::bin() const
{
    return std::string((const char *)hash, SIZE);
}
