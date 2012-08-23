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

#include <stdbool.h>
#include <stdint.h>

#include <string>
#include <iostream>

#include "debug.h"
#include "oriutil.h"
#include "localrepo.h"

using namespace std;

extern LocalRepo repository;

/*
 * Verify the repository.
 */
int
cmd_verify(int argc, const char *argv[])
{
    int status = 0;
    string error;
    set<ObjectInfo> objects = repository.listObjects();
    set<ObjectInfo>::iterator it;

    for (it = objects.begin(); it != objects.end(); it++)
    {
	error = repository.verifyObject((*it).hash);
	if (error != "") {
	    cout << "Object " << (*it).hash.hex() << endl;
	    cout << error << endl;
	    status = 1;
	}
    }

    return status;
}

