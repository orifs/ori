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

#define __STDC_LIMIT_MACROS

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>

#include <openssl/sha.h>

#ifdef OPENSSL_NO_SHA256
#error "SHA256 not supported!"
#endif

#include "tuneables.h"

#include <oriutil/debug.h>
#include <oriutil/stream.h>
#include <oriutil/objectinfo.h>

using namespace std;

ObjectInfo::ObjectInfo()
    : type(Null), flags(ORI_FLAG_DEFAULT), payload_size((uint32_t)-1)
{
}

ObjectInfo::ObjectInfo(const ObjectHash &hash)
    : type(Null), hash(hash), flags(ORI_FLAG_DEFAULT),
      payload_size((uint32_t)-1)
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
    ss.writeUInt32(flags);
    ss.writeUInt32(payload_size);

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
    flags = ss.readUInt32();
    payload_size = ss.readUInt32();
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

bool
ObjectInfo::isCompressed() const {
    return flags & ORI_FLAG_ZIPMASK == ORI_FLAG_UNCOMPRESSED;
}

ObjectInfo::ZipAlgo
ObjectInfo::getAlgo() const {
    switch (flags & ORI_FLAG_ZIPMASK) {
        case ORI_FLAG_UNCOMPRESSED:
            return ZIPALGO_NONE;
        case ORI_FLAG_FASTLZ:
            return ZIPALGO_FASTLZ;
        case ORI_FLAG_LZMA:
            return ZIPALGO_LZMA;
        default:
            return ZIPALGO_UNKNOWN;
    }
}

void
ObjectInfo::setAlgo(ObjectInfo::ZipAlgo algo)
{
    switch (algo) {
        case ZIPALGO_NONE:
            flags |= ORI_FLAG_UNCOMPRESSED;
            break;
        case ZIPALGO_FASTLZ:
            flags |= ORI_FLAG_FASTLZ;
            break;
        case ZIPALGO_LZMA:
            flags |= ORI_FLAG_LZMA;
            break;
        case ZIPALGO_UNKNOWN:
        default:
            NOT_IMPLEMENTED(false);
            break;
    }
}

bool ObjectInfo::operator <(const ObjectInfo &other) const {
    if (hash < other.hash) return true;
    if (type < other.type) return true;
    if (flags < other.flags) return true;
    if (payload_size < other.payload_size) return true;
    return false;
}

#define OBJINFO_PRINTBUF	512
void ObjectInfo::print(ostream &outStream) const {
    char buf[OBJINFO_PRINTBUF];
    size_t len;

    len = snprintf(buf, OBJINFO_PRINTBUF,
		   "Object info for %s\n", hash.hex().c_str());
    len += snprintf(buf + len, OBJINFO_PRINTBUF - len,
		    "  type = %s  flags = %08X  payload_size = %u\n",
                    getStrForType(type), flags, payload_size);
    outStream.write(buf, len);
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

