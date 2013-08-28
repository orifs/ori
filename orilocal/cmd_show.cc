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

#include <ori/localrepo.h>

using namespace std;

extern LocalRepo repository;

int
cmd_show(int argc, char * const argv[])
{
    string rootPath = LocalRepo::findRootPath();

    if (rootPath == "") {
        cout << "No repository found!" << endl;
        return 1;
    }

    cout << "--- Repository ---" << endl;
    cout << "Root: " << rootPath << endl;
    cout << "UUID: " << repository.getUUID() << endl;
    cout << "Version: " << repository.getVersion() << endl;
    cout << "HEAD: " << repository.getHead().hex() << endl;
    //printf("Peers:\n");
    // for
    // printf("    %s\n", hostname);
    return 0;
}

