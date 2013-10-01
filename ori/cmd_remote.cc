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

#include <ori/peer.h>
#include <ori/udsrepo.h>

using namespace std;

extern UDSRepo repository;

int
cmd_remote(int argc, char * const argv[])
{
    string val;
    strwstream req;

    req.writePStr("remote");

    if (argc == 1) {
        uint32_t num;

        req.writePStr("list");

        strstream resp = repository.callExt("FUSE", req.str());
        if (resp.ended()) {
            cout << "remote list failed with an unknown error!" << endl;
            return 1;
        }

        num = resp.readUInt32();

	// print peers
	cout << left << setw(16) << "Name"
	     << left << setw(64) << "Path" << "\nID" << endl;
	for (uint32_t i = 0; i < num; i++)
	{
            string name, blob;
            Peer p = Peer();

            resp.readLPStr(name);
            resp.readLPStr(blob);
            p.fromBlob(blob);
	    cout << left << setw(16) << name
		 << left << setw(64) << p.getUrl() << "\n" << p.getRepoId() << endl;
	}

	return 0;
    }

    if (argc == 4 && strcmp(argv[1], "add") == 0) {
        req.writePStr("add");
        req.writeLPStr(argv[2]);
        req.writeLPStr(argv[3]);

        strstream resp = repository.callExt("FUSE", req.str());
        if (resp.ended()) {
            cout << "remote add failed with an unknown error!" << endl;
            return 1;
        }

        resp.readLPStr(val);
        if (val != "")
            cout << val << endl;

	return 0;
    }

    if (argc == 3 && strcmp(argv[1], "del") == 0) {
        req.writePStr("del");
        req.writeLPStr(argv[2]);

        strstream resp = repository.callExt("FUSE", req.str());
        if (resp.ended()) {
            cout << "remote del failed with an unknown error!" << endl;
            return 1;
        }

        resp.readLPStr(val);
        if (val != "")
            cout << val << endl;

	return 0;
    }

    printf("Specify a command.\n");
    printf("usage: ori remote - Show the listings\n");
    printf("usage: ori remote add [NAME] [PATH] - Add a remote repository\n");
    printf("usage: ori remote del [NAME] - Delete a remote repository\n");

    return 1;
}

