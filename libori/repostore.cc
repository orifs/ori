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

#include <string>
#include <set>
#include <iostream>

#include "tuneables.h"

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/oristr.h>
#include <oriutil/orifile.h>
#include <oriutil/scan.h>

#include <ori/repostore.h>

using namespace std;

string
RepoStore_GetRepoPath(const string &fsName)
{
    if (!OriFile_Exists(Util_GetHome() + REPOSTORE_ROOTDIR)) {
        OriFile_MkDir(Util_GetHome() + REPOSTORE_ROOTDIR);
    }

    return Util_GetHome() + REPOSTORE_ROOTDIR"/" + fsName + ".ori";
}

int
getRepoHelper(set<string> *repos, const std::string &path)
{
    string fsName = OriFile_Basename(path);

    if (OriStr_EndsWith(fsName , ".ori"))
    {
        fsName = fsName.substr(0, fsName.size() - 4);
        repos->insert(fsName);
    }

    return 0;
}

set<string>
RepoStore_GetRepos()
{
    set<string> repos;

    DirIterate(Util_GetHome() + REPOSTORE_ROOTDIR, &repos, getRepoHelper);

    return repos;
}

