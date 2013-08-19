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

#include <oriutil/debug.h>
#include <oriutil/orifile.h>
#include <ori/localrepo.h>

using namespace std;

void
usage_init()
{
    cout << "ori init [OPTIONS] PATH" << endl;
    cout << endl;
    cout << "Create a new repository, by default this is a bare" << endl;
    cout << "repository that cannot be checked out through the CLI." << endl;
    cout << "For systems without FUSE support you can use the" << endl;
    cout << "--non-bare flag to force a full repository to be" << endl;
    cout << "created." << endl;
    cout << endl;
    cout << "Options:" << endl;
    cout << "    --non-bare     Non-bare repository" << endl;
}

/*
 * Create a new repository.
 *
 * TODO: Destroy partially created repository to handle errors better.
 */
int
cmd_init(int argc, char * const argv[])
{
    int ch;
    string rootPath;
    bool bareRepo = true;
    
    struct option longopts[] = {
        { "non-bare",   no_argument,    NULL,   'n' },
        { NULL,         0,              NULL,   0   }
    };

    while ((ch = getopt_long(argc, argv, "n", longopts, NULL)) != -1) {
        switch (ch) {
            case 'n':
                bareRepo = false;
                break;
            default:
                printf("usage: ori init [OPTIONS] PATH\n");
                return 1;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc == 0) {
        char *cwd = getcwd(NULL, MAXPATHLEN);
        rootPath = cwd;
        free(cwd);
    } else if (argc == 1) {
        rootPath = argv[0];
        if (!OriFile_Exists(rootPath)) {
            mkdir(rootPath.c_str(), 0755);
        } else {
            if (!OriFile_IsDirectory(rootPath)) {
                printf("The specified path exists, but is not a directory!\n");
                return 1;
            }
        }
    } else {
        printf("Too many arguments!\n");
        return 1;
    }

    return LocalRepo_Init(rootPath, bareRepo);
}

