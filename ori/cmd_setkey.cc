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
#include <iostream>

#include <oriutil/orifile.h>
#include <oriutil/key.h>
#include <ori/localrepo.h>

using namespace std;

extern LocalRepo repository;

int
cmd_setkey(int argc, char * const argv[])
{
    int status;
    string rootPath = LocalRepo::findRootPath();

    if (rootPath == "") {
        cout << "No repository found!" << endl;
        return 1;
    }

    if (argc != 2)
    {
	cout << "Specify the path to your private key." << endl;
	cout << "usage: ori setkey <private_key>" << endl;
    }

    switch (Key_GetType(argv[1]))
    {
	case KeyType::Invalid:
	    cout << "File not found or invalid key." << endl;
	    return 1;
	case KeyType::Public:
	    cout << "This appears to be a private key please specify a public key."
		 << endl;
	    return 1;
	case KeyType::Private:
	default:
	    break;
    }

    PrivateKey priv = PrivateKey();
    try {
        priv.open(argv[1]);
    } catch (exception e) {
	cout << "It appears that the key is invalid." << endl;
	return 1;
    }

    status = OriFile_Copy(argv[1], rootPath + ORI_PATH_PRIVATEKEY);
    if (status < 0)
    {
	cout << "Failed to copy the private key into the repository." << endl;
    }

    return 0;
}

