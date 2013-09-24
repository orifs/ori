/*
 * Copyright (c) 2012-2013 Stanford University
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
#include <ori/repostore.h>
#include <ori/localrepo.h>

using namespace std;

void
usage_newfs()
{
    cout << "ori newfs [OPTIONS] FSNAME" << endl;
    cout << endl;
    cout << "Create a new repository, this is a bare repository stored" << endl;
    cout << "stored in your home directory under '.ori'." << endl;
    //cout << endl;
    //cout << "Options:" << endl;
    //cout << "    --autosync     Enable autosync" << endl;
}

/*
 * Create a new file system.
 *
 * TODO: Destroy repository's that have failed to be created.
 */
int
cmd_newfs(int argc, char * const argv[])
{
    int ch;
    int status;
    bool autosync = false;
    string fsName;
    string rootPath;

    struct option longopts[] = {
        { "autosync",   no_argument,    NULL,   'a' },
        { NULL,         0,              NULL,   0   }
    };

    while ((ch = getopt_long(argc, argv, "a", longopts, NULL)) != -1) {
        switch (ch) {
            case 'a':
                autosync = true;
                break;
            default:
                printf("Usage: ori newfs [OPTIONS] FSNAME\n");
                return 1;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 1) {
        if (argc == 0)
            printf("Argument required!\n");
        if (argc > 1)
            printf("Too many arguments!\n"); 
        printf("Usage: ori newfs [OPTIONS] FSNAME\n");
        return 1;
    }
    
    fsName = argv[0];
    if (!Util_IsValidName(fsName)) {
        printf("Name contains invalid charecters!\n");
        return 1;
    }

    rootPath = RepoStore_GetRepoPath(fsName);
    if (OriFile_Exists(rootPath)) {
        printf("File system already exists!\n");
        return 1;
    }

    status = OriFile_MkDir(rootPath);
    if (status < 0) {
        perror("Failed to create directory");
        return 1;
    }
    status = LocalRepo_Init(rootPath, /* bareRepo */true);
    if (status != 0) {
        printf("Failed to create repository!\n");
        return 1;
    }

    // register with autosync
    if (autosync) {
        // XXX: Add to autosync
        NOT_IMPLEMENTED(false);
    }

    return 0;
}

