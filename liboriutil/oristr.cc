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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

#include <unistd.h>
#include <sys/types.h>

#include <iostream>
#include <vector>
#include <string>

#include "tuneables.h"

#include <oriutil/debug.h>
#include <oriutil/oristr.h>

using namespace std;

vector<string>
OriStr_Split(const string &str, char sep)
{
    size_t pos = 0;
    vector<string> rval;

    while (pos < str.length()) {
        size_t end = str.find(sep, pos);
        if (end == str.npos) {
            rval.push_back(str.substr(pos));
            break;
        }

        rval.push_back(str.substr(pos, end - pos));
        pos = end + 1;
    }

    return rval;
}

string
OriStr_Join(const vector<string> &str, char sep)
{
    int i;
    string rval = "";

    if (str.size() == 0)
        return rval;

    rval = str[0];
    for (i = 1; i < str.size(); i++) {
        rval += sep;
        rval += str[i];
    }

    return rval;
}

bool
OriStr_StartsWith(const std::string &str, const std::string &part)
{
    if (str.length() < part.length())
        return false;

    return str.substr(0, part.length()) == part;
}

bool
OriStr_EndsWith(const std::string &str, const std::string &part)
{
    if (str.length() < part.length())
        return false;

    return str.substr(str.length() - part.length(), part.length()) == part;
}

// XXX: Debug Only

#define TESTFILE_SIZE (HASHFILE_BUFSZ * 3 / 2)

int
OriStr_selfTest(void)
{
    string path;

    cout << "Testing OriStr ..." << endl;

    path = "hello.txt";
    ASSERT(OriStr_StartsWith(path, "hello"));
    ASSERT(OriStr_EndsWith(path, ".txt"));

    string sv_orig = "a,ab,abc";
    vector<string> sv = OriStr_Split(sv_orig, ',');
    ASSERT(sv[0] == "a");
    ASSERT(sv[1] == "ab");
    ASSERT(sv[2] == "abc");
    string sv_after = OriStr_Join(sv, ',');
    ASSERT(sv_orig == sv_after);

    return 0;
}

