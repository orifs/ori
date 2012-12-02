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

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>

#include <ori/debug.h>
#include <ori/oriutil.h>
#include <ori/localrepo.h>

using namespace std;

/*
 * Create a new repository.
 *
 * TODO: Destroy partially created repository to handle errors better.
 */
int
cmd_init(int argc, const char *argv[])
{
    string rootPath;
    
    if (argc == 1) {
        char *cwd = getcwd(NULL, MAXPATHLEN);
        rootPath = cwd;
        free(cwd);
    } else if (argc == 2) {
        rootPath = argv[1];
	if (!Util_FileExists(rootPath)) {
	    mkdir(rootPath.c_str(), 0755);
	} else {
	    if (!Util_IsDirectory(rootPath)) {
		printf("The specified path exists, but is not a directory!\n");
		return 1;
	    }
	}
    } else {
        printf("Too many arguments!\n");
        return 1;
    }

    return LocalRepo_Init(rootPath);
}

