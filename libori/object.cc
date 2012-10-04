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

#define __STDC_LIMIT_MACROS

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include <openssl/sha.h>

#ifdef OPENSSL_NO_SHA256
#error "SHA256 not supported!"
#endif

#include <ori/debug.h>
#include <ori/tuneables.h>
#include <ori/object.h>

using namespace std;

/*
 * ObjectInfo
 */
ObjectInfo::ObjectInfo()
    : type(Null), flags(0), payload_size((uint32_t)-1)
{
}

ObjectInfo::ObjectInfo(const ObjectHash &hash)
    : type(Null), hash(hash), flags(0), payload_size((uint32_t)-1)
{
}

std::string
ObjectInfo::toString() const
{
    strwstream ss;

    const char *type_str = getStrForType(type);
    if (type_str == NULL) {
        PANIC();
        return "";
    }
    ss.write(type_str, ORI_OBJECT_TYPESIZE);
    ss.writeHash(hash);
    ss.writeInt<uint32_t>(flags);
    ss.writeInt<uint32_t>(payload_size);

    ASSERT(ss.str().size() == SIZE);

    return ss.str();
}

void
ObjectInfo::fromString(const std::string &info)
{
    ASSERT(info.size() == SIZE);
    strstream ss(info);

    std::string type_str(ORI_OBJECT_TYPESIZE+1, '\0');
    ss.read((uint8_t*)&type_str[0], ORI_OBJECT_TYPESIZE);
    type = getTypeForStr(type_str.c_str());
    ASSERT(type != Null);

    ss.readHash(hash);
    flags = ss.readInt<uint32_t>();
    payload_size = ss.readInt<uint32_t>();
}

bool
ObjectInfo::hasAllFields() const
{
    if (type == Null)
        return false;
    if (hash.isEmpty())
        return false; // hash shouldn't be all zeros
    if (payload_size == (uint32_t)-1)
        return false; // no objects should be that large, due to LargeBlob
    return true;
}

bool ObjectInfo::getCompressed() const {
    return flags & ORI_FLAG_COMPRESSED;
}

bool ObjectInfo::operator <(const ObjectInfo &other) const {
    if (hash < other.hash) return true;
    if (type < other.type) return true;
    if (flags < other.flags) return true;
    if (payload_size < other.payload_size) return true;
    return false;
}

#define OBJINFO_PRINTBUF	512
void ObjectInfo::print(int fd) const {
    char buf[OBJINFO_PRINTBUF];
    size_t len;

    len = snprintf(buf, OBJINFO_PRINTBUF,
		   "Object info for %s\n", hash.hex().c_str());
    len += snprintf(buf + len, OBJINFO_PRINTBUF - len,
		    "  type = %s\n", getStrForType(type));
    len += snprintf(buf + len, OBJINFO_PRINTBUF - len,
		    "  flags = %08X\n", flags);
    len += snprintf(buf + len, OBJINFO_PRINTBUF - len,
		    "  payload_size = %u\n", payload_size);
    write(fd, buf, len);
}

const char *ObjectInfo::getStrForType(Type type) {
    const char *type_str = NULL;
    switch (type) {
        case Commit:    type_str = "CMMT"; break;
        case Tree:      type_str = "TREE"; break;
        case Blob:      type_str = "BLOB"; break;
        case LargeBlob: type_str = "LGBL"; break;
        case Purged:    type_str = "PURG"; break;
        default:
            printf("Unknown object type!\n");
            PANIC();
            return NULL;
    }
    return type_str;
}

ObjectInfo::Type ObjectInfo::getTypeForStr(const char *str) {
    if (strcmp(str, "CMMT") == 0) {
        return Commit;
    }
    else if (strcmp(str, "TREE") == 0) {
        return Tree;
    }
    else if (strcmp(str, "BLOB") == 0) {
        return Blob;
    }
    else if (strcmp(str, "LGBL") == 0) {
        return LargeBlob;
    }
    else if (strcmp(str, "PURG") == 0) {
        return Purged;
    }
    return Null;
}



/*
 * Object
 */
std::string Object::getPayload() {
    bytestream::ap bs(getPayloadStream());
    return bs->readAll();
}


