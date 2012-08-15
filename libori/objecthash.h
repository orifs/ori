#ifndef __OBJECTHASH_H__
#define __OBJECTHASH_H__

#include <stdint.h>

#include <tr1/unordered_map>

struct ObjectHash {
    static const size_t SIZE = 32;

    ObjectHash();
    static ObjectHash fromHex(std::string hex);

    bool operator <(const ObjectHash &other) const;
    bool operator ==(const ObjectHash &other) const;
    bool operator !=(const ObjectHash &other) const {
        return !(*this == other);
    }

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

namespace std { namespace tr1 {
    template<>
    struct hash<ObjectHash> : public unary_function<ObjectHash, size_t> {
        std::size_t operator()(const ObjectHash &key) const {
            return *((std::size_t*)key.hash);
        }
    };
} }

#endif
