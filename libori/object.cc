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

#include <assert.h>
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

#include "debug.h"
#include "tuneables.h"
#include "object.h"

using namespace std;

/*
 * ObjectInfo
 */
ObjectInfo::ObjectInfo()
    : type(Object::Null), flags(0), payload_size((size_t)-1)
{
    hash.resize(SHA256_DIGEST_LENGTH*2);
}

ObjectInfo::ObjectInfo(const char *in_hash)
    : type(Object::Null), flags(0), payload_size(0)
{
    hash.resize(SHA256_DIGEST_LENGTH*2);
    memcpy(&hash[0], in_hash, hash.size());
}

std::string
ObjectInfo::getInfo() const
{
    string rval;
    char buf[16];
    const char *type_str = Object::getStrForType(type);
    if (type_str == NULL) {
        assert(false);
        return "";
    }

    strncpy(buf, type_str, ORI_OBJECT_TYPESIZE);
    memcpy(buf+4, &flags, ORI_OBJECT_FLAGSSIZE);
    memcpy(buf+8, &payload_size, ORI_OBJECT_SIZE);

    rval.assign(buf, 16);

    return rval;
}

void
ObjectInfo::setInfo(const std::string &info)
{
    char buf[16];

    assert(info.size() == 16);
    memcpy(buf, info.c_str(), 16);

    memcpy(&flags, buf+4, ORI_OBJECT_FLAGSSIZE);
    memcpy(&payload_size, buf+8, ORI_OBJECT_SIZE);
    buf[4] = '\0';
    type = getTypeForStr(buf);
    assert(type != Object::Null);
}

ssize_t ObjectInfo::writeTo(int fd, bool seekable) {
    ssize_t status;
    char header[24];
    const char *type_str = Object::getStrForType(type);

    if (type_str == NULL)
        return -1;

    if (seekable) {
	memcpy(header, type_str, ORI_OBJECT_TYPESIZE);
	memcpy(header + 4, &flags, ORI_OBJECT_FLAGSSIZE);
	memcpy(header + 8, &payload_size, ORI_OBJECT_SIZE);
	memset(header + 16, 0, ORI_OBJECT_SIZE);

        status = pwrite(fd, header, 24, 0);
        if (status < 0) return status;
        assert(status == 24);

        return status;
    }
    else {
        // TODO!!! use write() instead
        assert(false);
    }
}

bool
ObjectInfo::hasAllFields() const
{
    if (type == Object::Null)
        return false;
    if (hash == "")
        return false; // hash shouldn't be all zeros
    if (payload_size == (size_t)-1)
        return false; // no objects should be that large, due to LargeBlob
    return true;
}

bool ObjectInfo::operator <(const ObjectInfo &other) const {
    if (hash < other.hash) return true;
    if (type < other.type) return true;
    if (flags < other.flags) return true;
    if (payload_size < other.payload_size) return true;
    return false;
}

bool ObjectInfo::getCompressed() const {
    return flags & ORI_FLAG_COMPRESSED;
}



/*
 * Object
 */
const char *Object::getStrForType(Type type) {
    const char *type_str = NULL;
    switch (type) {
        case Commit:    type_str = "CMMT"; break;
        case Tree:      type_str = "TREE"; break;
        case Blob:      type_str = "BLOB"; break;
        case LargeBlob: type_str = "LGBL"; break;
        case Purged:    type_str = "PURG"; break;
        default:
            printf("Unknown object type!\n");
            assert(false);
            return NULL;
    }
    return type_str;
}

Object::Type Object::getTypeForStr(const char *str) {
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

