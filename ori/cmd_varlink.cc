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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <set>
#include <string>
#include <iostream>
#include <iomanip>

#include <ori/udsrepo.h>

using namespace std;

extern UDSRepo repository;
    
int
cmd_varlink(int argc, char * const argv[])
{
    strwstream req;

    req.writePStr("varlink");

    // List variables
    if (argc == 1) {
        uint32_t num;
        string var, val;

        req.writePStr("list");

        strstream resp = repository.callExt("FUSE", req.str());
        if (resp.ended()) {
            cout << "varlink list failed with an unknown error!" << endl;
            return 1;
        }

        num = resp.readUInt32();

        cout << left << setw(16) << "Variable"
             << left << setw(64) << "Value" << endl;
        for (uint32_t i = 0; i < num; i++) {
            resp.readLPStr(var);
            resp.readLPStr(val);

            cout << left << setw(16) << var
                 << left << setw(64) << val << endl;
        }

        return 0;
    }

    // Get variable
    if (argc == 2) {
        string val;

        req.writePStr("get");
        req.writeLPStr(argv[1]);

        strstream resp = repository.callExt("FUSE", req.str());
        if (resp.ended()) {
            cout << "varlink get failed with an unknown error!" << endl;
            return 1;
        }

        resp.readLPStr(val);
        cout << val << endl;

        return 0;
    }

    // Set variable
    if (argc == 3) {
        req.writePStr("set");
        req.writeLPStr(argv[1]);
        req.writeLPStr(argv[2]);

        repository.callExt("FUSE", req.str());

        return 0;
    }

    printf("Wrong number of arguments!\n");
    printf("Usage: ori varlist - List variables\n");
    printf("Usage: ori varlist VARIABLE - Get variable\n");
    printf("Usage: ori varlist VARIABLE VALUE - Set variable\n");

    return 1;
}

