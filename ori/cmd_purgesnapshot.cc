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

#include <string>
#include <iostream>
#include <iomanip>

#include <ori/udsclient.h>
#include <ori/udsrepo.h>
#include <ori/remoterepo.h>
#include <ori/treediff.h>

using namespace std;

extern UDSRepo repository;

int
cmd_purgesnapshot(int argc, char * const argv[])
{
    if (argc != 2) {
	cout << "Error: Incorrect number of arguments." << endl;
	cout << "ori purgesnapshot <COMMITID>" << endl;
	return 1;
    }

    ObjectHash commitId = ObjectHash::fromHex(argv[1]);
    strwstream req;

    req.writePStr("purgesnapshot");
    req.writeUInt8(0); // Not time based
    req.writeHash(commitId);

    strstream resp = repository.callExt("FUSE", req.str());
    if (resp.ended()) {
	cout << "purgesnapshot failed with an unknown error!" << endl;
	return 1;
    }

    switch (resp.readUInt8())
    {
	case 0:
	{
	    cout << "Snapshot purged" << endl;
	    return 0;
	}
	case 1:
	case 2:
	{
	    string msg;
	    resp.readPStr(msg);
	    cout << msg << endl;
	}
	default:
	    NOT_IMPLEMENTED(false);
    }

    return 0;
}

