/*
 * Copyright (c) 2013 Stanford University
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

#include <cstdint>
#include <cinttypes>

#include <iostream>
#include <string>
#include <map>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/kvserializer.h>

using namespace std;

#define KVSERIALIZER_VERSIONSTR "KV00"

int
KVSerializer_selfTest()
{
    KVSerializer a;
    KVSerializer b;

    cout << "Testing KVSerializer ..." << endl;

    a.putStr("A", "1");
    a.putStr("B", "2");
    a.putStr("C", "3");
    a.putU8("D", 4);
    a.putU16("E", 0x1234);
    a.putU32("F", 0x12345678);
    a.putU64("G", 0x0123456789ABCDEFULL);

    b.fromBlob(a.getBlob());

    if (b.getStr("A") != "1") {
        cout << "Error getStr('A') failed!" << endl;
        return -1;
    }
    if (b.getStr("B") != "2") {
        cout << "Error getStr('B') failed!" << endl;
        return -1;
    }
    if (b.getStr("C") != "3") {
        cout << "Error getStr('C') failed!" << endl;
        return -1;
    }
    if (b.getU8("D") != 4) {
        cout << "Error getU8('D') failed!" << endl;
        return -1;
    }
    if (b.getU16("E") != 0x1234) {
        cout << "Error getU16('E') failed!" << endl;
        return -1;
    }
    if (b.getU32("F") != 0x12345678) {
        cout << "Error getU32('F') failed!" << endl;
        return -1;
    }
    if (b.getU64("G") != 0x0123456789ABCDEFULL) {
        cout << "Error getU64('G') failed!" << endl;
        return -1;
    }

    // Negative Tests
    bool fired = false;
    try {
        b.getU8("A");
    } catch (SerializationException &e) {
        fired = true;
    }
    if (!fired) {
        cout << "Failed to check type properly!" << endl;
        return -1;
    }

    try {
        b.getStr("BADKEY");
    } catch (exception &e) {
        fired = true;
    }
    if (!fired) {
        cout << "Failed to check key does not exist!" << endl;
        return -1;
    }

    return 0;
}

KVSerializer::KVSerializer()
{
}

KVSerializer::~KVSerializer()
{
}

void
KVSerializer::putStr(const std::string &key, const std::string &value)
{
    table[key] = "S" + value;
}

void
KVSerializer::putU8(const std::string &key, uint8_t value)
{
    string encoded = "BX";

    encoded[1] = value;
    table[key] = encoded;
}

void
KVSerializer::putU16(const std::string &key, uint16_t value)
{
    string encoded = "WXX";

    encoded[1] = (value >> 8) & 0xff;
    encoded[2] = value & 0xff;
    table[key] = encoded;
}

void
KVSerializer::putU32(const std::string &key, uint32_t value)
{
    string encoded = "DXXXX";

    encoded[1] = (value >> 24) & 0xff;
    encoded[2] = (value >> 16) & 0xff;
    encoded[3] = (value >> 8) & 0xff;
    encoded[4] = value & 0xff;
    table[key] = encoded;
}

void
KVSerializer::putU64(const std::string &key, uint64_t value)
{
    string encoded = "QXXXXXXXX";

    encoded[1] = (value >> 56) & 0xff;
    encoded[2] = (value >> 48) & 0xff;
    encoded[3] = (value >> 40) & 0xff;
    encoded[4] = (value >> 32) & 0xff;
    encoded[5] = (value >> 24) & 0xff;
    encoded[6] = (value >> 16) & 0xff;
    encoded[7] = (value >> 8) & 0xff;
    encoded[8] = value & 0xff;
    table[key] = encoded;
}

std::string
KVSerializer::getStr(const std::string &key) const
{
    const string encoded = table.at(key);

    if (encoded[0] != 'S')
        throw SerializationException("The value is not a string type");

    return encoded.substr(1);
}

uint8_t
KVSerializer::getU8(const std::string &key) const
{
    const string encoded = table.at(key);
    uint8_t val;

    if (encoded[0] != 'B')
        throw SerializationException("The value is not a uint8_t type");
    if (encoded.length() != 2)
        throw SerializationException("The value is incorrectly formatted");

    val = (uint8_t)encoded[1];

    return val;
}

uint16_t
KVSerializer::getU16(const std::string &key) const
{
    const string encoded = table.at(key);
    uint16_t val;

    if (encoded[0] != 'W')
        throw SerializationException("The value is not a uint16_t type");
    if (encoded.length() != 3)
        throw SerializationException("The value is incorrectly formatted");

    val = ((uint16_t)encoded[1] & 0xff) << 8;
    val |= (uint16_t)encoded[2] & 0xff;

    return val;
}

uint32_t
KVSerializer::getU32(const std::string &key) const
{
    const string encoded = table.at(key);
    uint32_t val;

    if (encoded[0] != 'D')
        throw SerializationException("The value is not a uint32_t type");
    if (encoded.length() != 5)
        throw SerializationException("The value is incorrectly formatted");

    val = ((uint32_t)encoded[1] & 0xff) << 24;
    val |= ((uint32_t)encoded[2] & 0xff) << 16;
    val |= ((uint32_t)encoded[3] & 0xff) << 8;
    val |= (uint32_t)encoded[4] & 0xff;

    return val;
}

uint64_t
KVSerializer::getU64(const std::string &key) const
{
    const string encoded = table.at(key);
    uint64_t val;

    if (encoded[0] != 'Q')
        throw SerializationException("The value is not a uint64_t type");
    if (encoded.length() != 9)
        throw SerializationException("The value is incorrectly formatted");

    val = ((uint64_t)encoded[1] & 0xff) << 56;
    val |= ((uint64_t)encoded[2] & 0xff) << 48;
    val |= ((uint64_t)encoded[3] & 0xff) << 40;
    val |= ((uint64_t)encoded[4] & 0xff) << 32;
    val |= ((uint64_t)encoded[5] & 0xff) << 24;
    val |= ((uint64_t)encoded[6] & 0xff) << 16;
    val |= ((uint64_t)encoded[7] & 0xff) << 8;
    val |= (uint64_t)encoded[8] & 0xff;

    return val;
}

KVSerializer::KVType
KVSerializer::getType(const std::string &key) const
{
    const string encoded = table.at(key);

    if (encoded[0] == 'S')
        return KVTypeString;
    if (encoded[0] == 'B')
        return KVTypeU8;
    if (encoded[0] == 'W')
        return KVTypeU16;
    if (encoded[0] == 'D')
        return KVTypeU32;
    if (encoded[0] == 'Q')
        return KVTypeU64;

    return KVTypeNull;
}

bool
KVSerializer::hasKey(const std::string &key) const
{
    map<string, string>::const_iterator it = table.find(key);
    return it != table.end();
}

void
KVSerializer::remove(const std::string &key)
{
    table.erase(key);
}

void
KVSerializer::removeAll()
{
    table.clear();
}

void
KVSerializer::fromBlob(const std::string &blob)
{
    int index = 0;
    int len = blob.length();

    if (len < 4) {
        throw SerializationException("Error invalid version");
    }

    if (blob.substr(0, 4) != KVSERIALIZER_VERSIONSTR) {
        throw SerializationException("Error unsupported version");
    }
    index = 4;

    while (index < len) {
        string key, value;
        uint16_t entryLen;
        if (len - index <= 4)
            throw SerializationException("Object invalid");

        entryLen = ((uint16_t)blob[index]) << 8;
        entryLen |= (uint16_t)blob[index+1];
        if (len - index < entryLen + 2)
            throw SerializationException("Error parsing key length");
        key = blob.substr(index+2, entryLen);
        index += 2 + entryLen;

        entryLen = ((uint16_t)blob[index]) << 8;
        entryLen |= (uint16_t)blob[index+1];
        if (len - index < entryLen)
            throw SerializationException("Error parsing value length");
        value = blob.substr(index+2, entryLen);
        index += 2 + entryLen;

        table[key] = value;
    }
}

string
KVSerializer::getBlob() const
{
    string blob;
    map<string, string>::const_iterator it;

    blob = KVSERIALIZER_VERSIONSTR;

    for (auto &it : table)
    {
        string entry;
        uint8_t lenH, lenL;

        // length must be less than size
        NOT_IMPLEMENTED(it.first.length() <= 0x0000FFFF);
        lenH = it.first.length() >> 8;
        lenL = it.first.length() & 0x00FF;
        entry = lenH;
        entry += lenL;
        entry += it.first;

        NOT_IMPLEMENTED(it.second.length() <= 0x0000FFFF);
        lenH = it.second.length() >> 8;
        lenL = it.second.length() & 0x00FF;
        entry += lenH;
        entry += lenL;
        entry += it.second;

        blob += entry;
    }

    return blob;
}

void
KVSerializer::dump() const
{
    map<string, string>::const_iterator it;

    cout << "KVSerializer Dump:" << endl;
    for (auto &it : table)
    {
        KVType type = getType(it.first);
        char buf[32];

        if (type == KVTypeString) {
            cout << it.first << ": " << getStr(it.first) << endl;
        } else if (type == KVTypeU8) {
            snprintf(buf, sizeof(buf), "%u", getU8(it.first));
            cout << it.first << ": " << buf << endl;
        } else if (type == KVTypeU16) {
            snprintf(buf, sizeof(buf), "%u", getU16(it.first));
            cout << it.first << ": " << buf << endl;
        } else if (type == KVTypeU32) {
            snprintf(buf, sizeof(buf), "%u", getU32(it.first));
            cout << it.first << ": " << buf << endl;
        } else if (type == KVTypeU64) {
            snprintf(buf, sizeof(buf), "%" PRIu64, getU64(it.first));
            cout << it.first << ": " << buf << endl;
        } else {
            cout << "Unknown type!" << endl;
        }

    }
}

