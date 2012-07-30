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

#include <assert.h>
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
 * Graft help
 */
void
usage_graft(void)
{
    cout << "ori graft <Source Path> <Destination Directory>" << endl;
    cout << endl;
    cout << "Graft a subtree from a repository." << endl;
    cout << endl;
}

/*
 * Graft a subtree into a new tree with a patched history.
 *
 * XXX: Eventually we need to implement a POSIX compliant copy command.
 */
int
cmd_graft(int argc, const char *argv[])
{
    string srcRoot, dstRoot, srcRelPath, dstRelPath;
    LocalRepo srcRepo, dstRepo;

    if (argc != 3) {
        cout << "Error in correct number of arguments." << endl;
        cout << "ori graft <Source Path> <Destination Path>" << endl;
        return 1;
    }

    // Convert relative paths to full paths.
    srcRelPath = Util_RealPath(argv[1]);
    dstRelPath = Util_RealPath(argv[2]);

    if (srcRelPath == "" || dstRelPath == "") {
        cout << "Error: Unable to resolve relative paths." << endl;
        return 1;
    }

    srcRoot = LocalRepo::findRootPath(srcRelPath);
    dstRoot = LocalRepo::findRootPath(dstRelPath);

    if (srcRoot == "") {
        cout << "Error: source path is not a repository." << endl;
        return 1;
    }

    if (dstRoot == "") {
        cout << "Error: destination path is not a repository." << endl;
        return 1;
    }

    srcRepo.open(srcRoot);
    dstRepo.open(dstRoot);

    // Transform the paths to be relative to repository root.
    srcRelPath = srcRelPath.substr(srcRoot.length());
    dstRelPath = dstRelPath.substr(dstRoot.length());

    cout << srcRelPath << endl;
    cout << dstRelPath << endl;

    dstRepo.graftSubtree(&srcRepo, srcRelPath, dstRelPath);

    return 0;
}

