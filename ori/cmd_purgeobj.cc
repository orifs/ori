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
#include <iomanip>

#include "debug.h"
#include "util.h"
#include "localrepo.h"

using namespace std;

extern LocalRepo repository;

int
cmd_purgeobj(int argc, const char *argv[])
{
    if (argc != 2) {
	cout << "Error: Incorrect number of arguements." << endl;
	cout << "ori purgeobj <OBJID>" << endl;
	return 1;
    }

    ObjectHash objId = ObjectHash::fromHex(argv[1]);

    if (repository.getObjectType(objId) != Object::Blob) {
	cout << "Error: You can only purge an object with type Blob." << endl;
	return 1;
    }

    if (!repository.purgeObject(objId)) {
	cout << "Error: Failed to purge object." << endl;
	return 1;
    }

    return 0;
}

