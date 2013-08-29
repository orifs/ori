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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <iostream>

#include <ori/udsclient.h>
#include <ori/udsrepo.h>

#include "fuse_cmd.h"

using namespace std;

extern UDSRepo repository;

int
cmd_snapshots(int argc, char * const argv[])
{
    uint32_t len;
    strwstream req;

    req.writePStr("snapshots");
    strstream resp = repository.callExt("FUSE", req.str());
    if (resp.ended()) {
        cout << "snapshots failed with an unknown error!" << endl;
        return 1;
    }

    len = resp.readUInt8();
    for (uint32_t i = 0; i < len; i++)
    {
        string name;
        resp.readPStr(name);
        cout << name << endl;
    }

    return 0;
}

