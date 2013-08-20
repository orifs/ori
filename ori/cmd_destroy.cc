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

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/orifile.h>
#include <oriutil/scan.h>
#include <ori/repostore.h>

using namespace std;

void
usage_destroy()
{
    cout << "ori destroy FSNAME" << endl;
    cout << endl;
    cout << "Destroy a repository. This is a permanant operation local" << endl;
    cout << "to this machine" << endl;
}

static int
destroyHelper(int unused, const string &path)
{
    if (OriFile_IsDirectory(path))
        OriFile_RmDir(path);
    else
        OriFile_Delete(path);

    return 0;
}

/*
 * Destroy a repository.
 */
int
cmd_destroy(int argc, char * const argv[])
{
    string fsName;
    string rootPath;

    if (argc != 2) {
        if (argc == 1)
            printf("Argument required!\n");
        if (argc > 2)
            printf("Too many arguments!\n"); 
        printf("Usage: ori destroy FSNAME\n");
        return 1;
    }
    
    fsName = argv[1];
    if (!Util_IsValidName(fsName)) {
        printf("Name contains invalid charecters!\n");
        return 1;
    }

    rootPath = RepoStore_GetRepoPath(fsName);
    if (!OriFile_Exists(rootPath)) {
        printf("File system does not exist!\n");
        return 1;
    }

    // XXX: unregister with autosync

    DirRTraverse(rootPath, 0, destroyHelper);
    OriFile_RmDir(rootPath);

    return 0;
}

