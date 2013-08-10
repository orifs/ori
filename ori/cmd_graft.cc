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

#include <string>
#include <iostream>
#include <iomanip>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <ori/localrepo.h>

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
cmd_graft(int argc, char * const argv[])
{
    string srcRoot, dstRoot, srcRelPath, dstRelPath;
    string dstName = "";
    LocalRepo srcRepo, dstRepo;
    ObjectHash graftHead;

    if (argc != 3) {
        cout << "Error in correct number of arguments." << endl;
        cout << "ori graft <Source Path> <Destination Path>" << endl;
        return 1;
    }

    // Convert relative paths to full paths.
    srcRelPath = Util_RealPath(argv[1]);
    dstRelPath = Util_RealPath(argv[2]);

    if (srcRelPath == "") {
        cout << "Error: Source file or directory does not exist!" << endl;
	return 1;
    }

    if (dstRelPath == "") {
	// Try stripping last string and recompute
	string newDstPath = argv[2];
	if (newDstPath.find_last_of("/") == newDstPath.npos) {
	    dstName = argv[2];
	    dstRelPath = "./";
	} else {
	    dstName = newDstPath.substr(newDstPath.find_last_of("/") + 1);
	    dstRelPath = newDstPath.substr(0, newDstPath.find_last_of("/") + 1);
	}
	dstRelPath = Util_RealPath(dstRelPath);
	if (dstRelPath == "") {
	    cout << "Error: Unable to resolve relative paths." << endl;
	    return 1;
	}
    }

    dstRelPath = dstRelPath + "/";

    srcRoot = LocalRepo::findRootPath(srcRelPath);
    dstRoot = LocalRepo::findRootPath(dstRelPath);
    if (srcRoot[srcRoot.length() - 1] != '/')
	srcRoot = srcRoot + "/";
    if (dstRoot[dstRoot.length() - 1] != '/')
	dstRoot = dstRoot + "/";

    if (srcRoot == "/" || dstRoot == "/") {
        cout << "Warning: source or destination is not a repository." << endl;
        string dstFile = dstRelPath + "/" + dstName;

        if (Util_IsDirectory(srcRelPath)) {
            execl("/bin/cp", "cp", "-r", srcRelPath.c_str(), dstFile.c_str(),
                  (char *)NULL);
        } else {
            execl("/bin/cp", "cp", srcRelPath.c_str(), dstFile.c_str(),
                  (char *)NULL);
        }

        return 0;
    }

    srcRepo.open(srcRoot);
    dstRepo.open(dstRoot);

    // Transform the paths to be relative to repository root.
    srcRelPath = srcRelPath.substr(srcRoot.length() - 1);
    dstRelPath = dstRelPath.substr(dstRoot.length() - 1);
    dstRelPath = dstRelPath + dstName;

    // Append name if it's not specified
    if (dstRelPath[dstRelPath.size() - 1] == '/') {
	dstName = srcRelPath.substr(srcRelPath.find_last_of("/") + 1);
	dstRelPath = dstRelPath + dstName;
    }

    cout << "Grafting from " << srcRelPath << " to " << dstRelPath << endl;

    graftHead = dstRepo.graftSubtree(&srcRepo, srcRelPath, dstRelPath);
    if (graftHead.isEmpty()) {
	printf("Error: Could not find a file or directory with this name!");
	return 1;
    }

    repository.updateHead(graftHead);

    return 0;
}

