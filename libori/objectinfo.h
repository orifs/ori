#ifndef __OBJECTINFO_H__
#define __OBJECTINFO_H__

#include <string>

#include "objecthash.h"

struct ObjectInfo {
    enum Type { Null, Commit, Tree, Blob, LargeBlob, Purged };

    ObjectInfo();
    ObjectInfo(const ObjectHash &hash);

    std::string toString() const;
    void fromString(const std::string &info);
    //ssize_t writeTo(int fd, bool seekable = true);
    bool hasAllFields() const;

    // Flags operations
    bool getCompressed() const;
    bool operator <(const ObjectInfo &) const;

    // Object type
    static const char *getStrForType(Type t);
    static Type getTypeForStr(const char *str);

    // For debug use
    void print(int fd = STDOUT_FILENO) const;

    Type type;
    ObjectHash hash;
    uint32_t flags;
    uint32_t payload_size;

    static const size_t SIZE = 4+ObjectHash::SIZE+2*sizeof(uint32_t);
};

typedef ObjectInfo::Type ObjectType;

#endif
