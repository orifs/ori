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

#include <stdint.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <getopt.h>

#include <string>
#include <iostream>
#include <iomanip>

#include <oriutil/debug.h>
#include <oriutil/scan.h>
#include <oriutil/orifile.h>
#include <oriutil/oricrypt.h>

#include <ori/udsclient.h>
#include <ori/udsrepo.h>

using namespace std;

extern UDSRepo repository;

void
usage_checkout()
{
    cout << "ori checkout [OPTIONS] [COMMITID]" << endl;
    cout << endl;
    cout << "Checkout a file system snapshot, the force checkout flag" << endl;
    cout << "will cause the current changes to be lost.  If the" << endl;
    cout << "COMMITID is omitted this can be used with the force flag" << endl;
    cout << "to discard any changes." << endl;
    cout << endl;
    cout << "Options:" << endl;
    cout << "    --force        Force checkout" << endl;
}

int
cmd_checkout(int argc, char * const argv[])
{
    int ch;
    bool force = false;
    Commit c;
    ObjectHash tip = repository.getHead();

    struct option longopts[] = {
        { "force",      no_argument,    NULL,   'f' },
        { NULL,         0,              NULL,   0   }
    };

    while ((ch = getopt_long(argc, argv, "f", longopts, NULL)) != -1) {
        switch (ch) {
            case 'f':
                force = true;
                break;
            default:
                printf("Usage: ori checkout [OPTIONS] COMMITID\n");
                return 1;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc == 1) {
        tip = ObjectHash::fromHex(argv[0]);
    }

    if (tip != EMPTY_COMMIT) {
        c = repository.getCommit(tip);
    }

    strwstream req;

    req.writePStr("checkout");
    req.writeHash(tip);
    req.writeUInt8(force ? 1 : 0);


    strstream resp = repository.callExt("FUSE", req.str());
    if (resp.ended()) {
        cout << "checkout failed with an unknown error!" << endl;
        return 1;
    }

    uint8_t result = resp.readUInt8();

    if (result == 0) {
        string error;
        resp.readPStr(error);
        cout << "Checkout failed: " << error << endl;
        return 1;
    }

    cout << "Checkout success!" << endl;

    return 0;
}

