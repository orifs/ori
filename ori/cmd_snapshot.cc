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

#include <getopt.h>

#include <string>
#include <iostream>

#include <ori/udsclient.h>
#include <ori/udsrepo.h>

#include "fuse_cmd.h"

using namespace std;

extern UDSRepo repository;

void
usage_snapshot(void)
{
    cout << "ori snapshot [OPTIONS] [SNAPSHOT NAME]" << endl;
    cout << endl;
    cout << "Create a snapshot of the file system." << endl;
    cout << endl;
    cout << "An additional snapshot name may be set, but it may only" << endl;
    cout << "contain letters, numbers, underscores, and periods." << endl;
    cout << endl;
    cout << "Options:" << endl;
    cout << "    -m message     Add a message to the snapshot" << endl;
}

int
cmd_snapshot(int argc, char * const argv[])
{
    int ch;
    bool hasMsg = false;
    bool hasName = false;
    string msg;
    string name;

    struct option longopts[] = {
        { "message",    required_argument,  NULL,   'm' },
        { NULL,         0,                  NULL,   0   }
    };

    while ((ch = getopt_long(argc, argv, "m:", longopts, NULL)) != -1) {
        switch (ch) {
            case 'm':
                hasMsg = true;
                msg = optarg;
                break;
            default:
                printf("Usage: ori snapshot [OPTIONS] [SNAPSHOT NAME]\n");
                return 1;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 0 && argc != 1) {
        cout << "Wrong number of arguments." << endl;
        cout << "Usage: ori snapshot [OPTIONS] [SNAPSHOT NAME]" << endl;
        return 1;
    }

    if (argc == 1) {
        hasName = true;
        name = argv[0];
    }

    if (hasName && !hasMsg) {
        msg = "Created snapshot '" + name + "'";
        hasMsg = true;
    }

    strwstream req;

    req.writePStr("snapshot");
    req.writeUInt8(hasMsg ? 1 : 0);
    req.writeUInt8(hasName ? 1 : 0);
    if (hasMsg)
        req.writeLPStr(msg);
    if (hasName)
        req.writeLPStr(name);

    strstream resp = repository.callExt("FUSE", req.str());
    if (resp.ended()) {
        cout << "status failed with an unknown error!" << endl;
        return 1;
    }

    switch (resp.readUInt8())
    {
        case 0:
            cout << "No changes" << endl;
            return 0;
        case 1:
        {
            ObjectHash hash;
            resp.readHash(hash);
            cout << "Committed " << hash.hex() << endl;
            return 0;
        }
        default:
            NOT_IMPLEMENTED(false);
    }

    return 0;
}

