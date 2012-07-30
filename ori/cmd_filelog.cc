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
cmd_filelog(int argc, const char *argv[])
{
    string commit = repository.getHead();
    list<pair<Commit, string> > revs;
    list<pair<Commit, string> >::iterator it;
    Commit lastCommit;
    string lastCommitHash;
    string lastHash = "";

    if (argc != 2) {
	cout << "Wrong number of arguments!" << endl;
	return 1;
    }

    while (commit != EMPTY_COMMIT) {
	Commit c = repository.getCommit(commit);
	string objId;

	objId = repository.lookup(c, argv[1]);

	if (lastHash != objId && lastHash != "") {
	    revs.push_back(make_pair(lastCommit, lastCommitHash));
	}
	lastCommit = c;
	lastCommitHash = commit;
	lastHash = objId;

	commit = c.getParents().first;
	// XXX: Handle merge cases
    }

    if (lastHash != "") {
	revs.push_back(make_pair(lastCommit, lastCommitHash));
    }

    for (it = revs.begin(); it != revs.end(); it++) {
	Commit c = (*it).first;
	time_t timeVal = c.getTime();
	char timeStr[26];

	ctime_r(&timeVal, timeStr);

	cout << "Commit:  " << (*it).second << endl;
	cout << "Parents: " << c.getParents().first << endl;
	cout << "Author:  " << c.getUser() << endl;
	// XXX: print file id?
	cout << "Date:    " << timeStr << endl;
	cout << c.getMessage() << endl << endl;
    }

    return 0;
}

