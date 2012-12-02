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

#include <ori/debug.h>
#include <ori/localrepo.h>

using namespace std;

extern LocalRepo repository;

int
cmd_listkeys(int argc, const char *argv[])
{
    string rootPath = LocalRepo::findRootPath();

    if (rootPath == "") {
        cout << "No repository found!" << endl;
        return 1;
    }

    map<string, PublicKey> keys = repository.getPublicKeys();
    map<string, PublicKey>::iterator it;

    cout << left << setw(35) << "E-mail" << "Fingerprint" << endl;
    for (it = keys.begin(); it != keys.end(); it++)
    {
	cout << left << setw(35) << (*it).second.getEmail()
	     << (*it).first << endl;
    }

    return 0;
}

