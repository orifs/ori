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

#include <stdbool.h>
#include <stdint.h>

#include <iostream>
#include <string>
#include <map>

#include <ori/debug.h>
#include <ori/oriutil.h>
#include <ori/kvserializer.h>

using namespace std;

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

KVSerializer::KVType
KVSerializer::getType(const std::string &key) const
{
    const string encoded = table.at(key);

    if (encoded[0] == 'S')
        return KVTypeString;
    if (encoded[0] == 'B')
        return KVTypeU8;

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

    while (index < len) {
        string key, value;
        uint16_t entryLen;
        if (len - index <= 4)
            throw SerializationException("Object invalid");

        entryLen = ((uint8_t)blob[index]) << 8;
        entryLen |= (uint8_t)blob[index+1];
        if (len - index < entryLen + 2)
            throw SerializationException("Error parsing key length");
        key = blob.substr(index+2, entryLen);
        index += 2 + entryLen;

        entryLen = ((uint8_t)blob[index]) << 8;
        entryLen |= (uint8_t)blob[index+1];
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

    for (it = table.begin(); it != table.end(); it++)
    {
        string entry;
        uint8_t lenH, lenL;

        // length must be less than size
        NOT_IMPLEMENTED(it->first.length() > 0x0000FFFF);
        lenH = it->first.length() >> 8;
        lenL = it->first.length() & 0x00FF;
        entry = lenH;
        entry += lenL;
        entry += it->first;

        NOT_IMPLEMENTED(it->second.length() > 0x0000FFFF);
        lenH = it->second.length() >> 8;
        lenL = it->second.length() & 0x00FF;
        entry += lenH;
        entry += lenL;
        entry += it->second;

        blob += entry;
    }

    return blob;
}

