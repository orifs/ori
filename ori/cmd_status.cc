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

#include <unistd.h>

#include <string>
#include <iostream>
#include <iomanip>

#include <ori/udsclient.h>
#include <ori/udsrepo.h>

#include "fuse_cmd.h"

using namespace std;

extern UDSRepo repository;

int
cmd_status(int argc, char * const argv[])
{
    strwstream req;

    req.writePStr("status");

    strstream resp = repository.callExt("FUSE", req.str());
    if (resp.ended()) {
        cout << "status failed with an unknown error!" << endl;
        return 1;
    }

    uint32_t len = resp.readUInt32();
    for (size_t i = 0; i < len; i++) {
        char type = resp.readUInt8();
        string path;

        resp.readLPStr(path);

        printf("%c   %s\n", type, path.c_str());
    }

    return 0;
}

