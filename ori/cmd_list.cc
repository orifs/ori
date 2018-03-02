/*
 * Copyright (c) 2013 Stanford University
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

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <getopt.h>

#include <string>
#include <iostream>
#include <iomanip>

#include <oriutil/debug.h>
#include <oriutil/orifile.h>
#include <ori/repostore.h>
#include <ori/localrepo.h>

using namespace std;

void
usage_list()
{
    cout << "ori list" << endl;
    cout << endl;
    cout << "List all local file systems." << endl;
}

/*
 * List local file systems.
 */
int
cmd_list(int argc, char * const argv[])
{
    set<string> repos = RepoStore_GetRepos();

    cout << left << setw(32) << "Name" << "File System ID" << endl;
    for (auto &it : repos) {
        string path, id;

        path = RepoStore_GetRepoPath(it) + ORI_PATH_UUID;
        try {
            id = OriFile_ReadFile(path);
        } catch (exception &e) {
            id = "*Corrupt or Uninitialized*";
        }
        
        cout << left << setw(32) << it << id << endl;
    }

    return 0;
}

