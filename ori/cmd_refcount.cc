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

/*
 * Print the reference counts for all objects, or show the references
 * to a particular object.
 */
int
cmd_refcount(int argc, const char *argv[])
{
    if (argc == 1) {
        map<string, map<string, Object::BRState> > refs;
        map<string, map<string, Object::BRState> >::iterator it;

        refs = repository.getRefCounts();

        cout << left << setw(64) << "Object" << " Count" << endl;
        for (it = refs.begin(); it != refs.end(); it++) {
            cout << (*it).first << " " << (*it).second.size() << endl;
        }
    } else if (argc == 2) {
        map<string, Object::BRState> refs;
        map<string, Object::BRState>::iterator it;

        refs = repository.getRefs(argv[1]);

        for (it = refs.begin(); it != refs.end(); it++) {
            cout << (*it).first << endl;
        }
    } else {
        cout << "Invalid number of arguements." << endl;
        cout << "ori refcount [OBJID]" << endl;
    }

    return 0;
}

