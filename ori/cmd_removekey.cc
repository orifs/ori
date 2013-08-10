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

#include <string>
#include <iomanip>
#include <iostream>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <ori/localrepo.h>

using namespace std;

extern LocalRepo repository;

int
cmd_removekey(int argc, char * const argv[])
{
    string rootPath = LocalRepo::findRootPath();

    if (rootPath == "") {
        cout << "No repository found!" << endl;
        return 1;
    }

    if (argc != 2) {
	cout << "Please specify the fingerprint of the key to remove!" << endl;
	return 1;
    }

    map<string, PublicKey> keys = repository.getPublicKeys();
    map<string, PublicKey>::iterator it;

    it = keys.find(argv[1]);
    if (it == keys.end()) {
	cout << "Key not found!" << endl;
	return 1;
    }

    if (Util_DeleteFile(rootPath + ORI_PATH_TRUSTED + argv[1]) < 0) {
	cout << "Failed to delete key!" << endl;
	return 1;
    }

    return 0;
}

