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

#include <assert.h>
#include <stdio.h>

#include <cstdlib>

#include <unistd.h>
#include <sys/stat.h>

#include <iostream>
#include <sstream>
#include <string>

#include "largeblob.h"

using namespace std;

/********************************************************************
 *
 *
 * LBlobEntry
 *
 *
 ********************************************************************/

LBlobEntry::LBlobEntry(const LBlobEntry &l)
    : hash(l.hash), length(l.length)
{
}

LBlobEntry::LBlobEntry(const std::string &h, uint16_t l)
    : hash(h), length(l)
{
}

LBlobEntry::~LBlobEntry()
{
}

/********************************************************************
 *
 *
 * LargeBlob
 *
 *
 ********************************************************************/

LargeBlob::LargeBlob()
{
}

LargeBlob::~LargeBlob()
{
}

void
LargeBlob::chunkFile(const string &path)
{
}

void
LargeBlob::extractFile(const string &path)
{
}

const string
LargeBlob::getBlob()
{
    string blob = "";
    map<uint64_t, LBlobEntry>::iterator it;

    for (it = parts.begin(); it != parts.end(); it++)
    {
	blob += (*it).second.hash + " ";
	blob += (*it).second.length;
	blob += "\n";
    }

    return blob;
}

void
LargeBlob::fromBlob(const string &blob)
{
    uint64_t off = 0;
    string line;
    stringstream ss(blob);

    while (getline(ss, line, '\n')) {
        string hash;
        string length;

	hash = line.substr(0, 64);
        length = line.substr(65);

	parts.insert(make_pair(off,
		    LBlobEntry(hash, strtoul(length.c_str(), NULL, 10))));
    }
}

