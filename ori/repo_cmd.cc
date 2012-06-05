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

#define _WITH_DPRINTF

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>

#include "debug.h"
#include "scan.h"
#include "util.h"
#include "repo.h"

using namespace std;

/********************************************************************
 *
 *
 * Commands
 *
 *
 ********************************************************************/


int
cmd_init(int argc, const char *argv[])
{
    string rootPath;
    string oriDir;
    string tmpDir;
    string objDir;
    string versionFile;
    string uuidFile;
#ifdef __FreeBSD__
    uuid_t id;
    char *uuidBuf;
#endif /* __FreeBSD__ */
    uint32_t status;
    int fd;
    
    if (argc == 1) {
        char *cwd = getcwd(NULL, MAXPATHLEN);
        rootPath = cwd;
        free(cwd);
    } else if (argc == 2) {
        rootPath = argv[1];
	if (!Util_IsDirectory(rootPath)) {
	    mkdir(rootPath.c_str(), 0755);
	}
    } else {
        printf("Too many arguments!\n");
        return 1;
    }

    // Create directory
    oriDir = rootPath + ORI_PATH_DIR;
    if (mkdir(oriDir.c_str(), 0755) < 0) {
        perror("Could not create '.ori' directory");
        return 1;
    }

    // Create tmp directory
    tmpDir = rootPath + ORI_PATH_DIR + "/tmp";
    if (mkdir(tmpDir.c_str(), 0755) < 0) {
        perror("Could not create '.ori/tmp' directory");
        return 1;
    }

    // Create objs directory
    tmpDir = rootPath + ORI_PATH_DIR + "/objs";
    if (mkdir(tmpDir.c_str(), 0755) < 0) {
        perror("Could not create '.ori/objs' directory");
        return 1;
    }

    // Construct UUID
    uuidFile = rootPath + ORI_PATH_UUID;
    fd = open(uuidFile.c_str(), O_CREAT|O_WRONLY|O_APPEND, 0660);
    if (fd < 0) {
        perror("Could not create UUID file");
        return 1;
    }

#ifdef __FreeBSD__
    uuid_create(&id, &status);
    if (status != uuid_s_ok) {
        printf("Failed to construct UUID!\n");
        return 1;
    }
    uuid_to_string(&id, &uuidBuf, &status);
    if (status != uuid_s_ok) {
        printf("Failed to print UUID!\n");
        return 1;
    }
    dprintf(fd, "%s", uuidBuf);
    free(uuidBuf);
#endif /* __FreeBSD__ */
    close(fd);
    chmod(uuidFile.c_str(), 0440);

    // Construct version file
    versionFile = rootPath + ORI_PATH_VERSION;
    fd = open(versionFile.c_str(), O_CREAT|O_WRONLY|O_APPEND, 0660);
    if (fd < 0) {
        perror("Could not create version file");
        return 1;
    }
    dprintf(fd, "ORI1.0");
    close(fd);

    return 0;
}

int
cmd_show(int argc, const char *argv[])
{
    string rootPath = Repo::getRootPath();

    if (rootPath.compare("") == 0) {
        printf("No repository found!\n");
        return 1;
    }

    printf("--- Repository ---\n");
    printf("Root: %s\n", rootPath.c_str());
    printf("UUID: %s\n", repository.getUUID().c_str());
    printf("Version: %s\n", repository.getVersion().c_str());
    //printf("Peers:\n");
    // for
    // printf("    %s\n", hostname);
    return 0;
}

int
cmd_catobj(int argc, const char *argv[])
{
    size_t len;
    const char *rawBuf;
    std::string buf;
    bool hex = false;

    len = repository.getObjectLength(argv[1]);
    if (len < 0) {
	printf("Object does not exist.\n");
	return 1;
    }

    buf = repository.getObject(argv[1]);
    rawBuf = buf.data();

    for (int i = 0; i < 0; i++) {
	if (rawBuf[i] < 1 || rawBuf[i] >= 0x80) {
	    hex = true;
	    break;
	}
    }

    if (hex) {
	// XXX: Add hex pretty printer
	printf("hex object!\n");
    } else {
	printf("%s", rawBuf);
    }

    return 0;
}

int
cmd_listobj(int argc, const char *argv[])
{
    set<string> objects = repository.getObjects();
    set<string>::iterator it;

    for (it = objects.begin(); it != objects.end(); it++)
    {
	printf("%s\n", (*it).c_str());
    }

    return 0;
}

int
commitHelper(void *arg, const char *path)
{
    string filePath = path;
    string hash;
    Tree *tree = (Tree *)arg;

    if (Util_IsDirectory(path)) {
	Tree subTree = Tree();

	Scan_Traverse(path, &subTree, commitHelper);

	hash = repository.addTree(subTree);
    } else {
	hash = repository.addFile(path);
    }
    tree->addObject(filePath.c_str(), hash);

    return 0;
}

int
cmd_commit(int argc, const char *argv[])
{
    string blob;
    string treeHash, commitHash;
    Tree tree = Tree();
    Commit commit = Commit();
    string root = Repo::getRootPath();

    Scan_Traverse(root.c_str(), &tree, commitHelper);

    treeHash = repository.addTree(tree);

    // XXX: Get parents
    commit.setTree(treeHash);
    commit.setParents(repository.getHead());
    commit.setMessage("test");

    commitHash = repository.addCommit(commit);

    // Update .ori/HEAD
    repository.updateHead(commitHash);

    printf("Commit Hash: %s\nTree Hash: %s\n%s",
	   commitHash.c_str(),
	   treeHash.c_str(),
	   blob.c_str());
}

int
StatusDirectoryCB(void *arg, const char *path)
{
    string repoRoot = Repo::getRootPath();
    string objPath = path;
    string objHash;
    map<string, string> *dirState = (map<string, string> *)arg;

    if (!Util_IsDirectory(objPath)) {
	objHash = Util_HashFile(objPath);
        objPath = objPath.substr(repoRoot.size());

	dirState->insert(pair<string, string>(objPath, objHash));
    } else {
	dirState->insert(pair<string, string>(objPath, "DIR"));
    }

    return 0;
}

int
StatusTreeIter(map<string, string> *tipState,
	       const string &path,
	       const string &treeId)
{
    Tree tree;
    map<string, TreeEntry>::iterator it;

    // XXX: Error handling
    tree = repository.getTree(treeId);

    for (it = tree.tree.begin(); it != tree.tree.end(); it++) {
	if ((*it).second.type == TreeEntry::Tree) {
	    tipState->insert(pair<string, string>(path + "/" + (*it).first,
						  "DIR"));
	    StatusTreeIter(tipState,
			   path + "/" + (*it).first,
			   (*it).second.hash);
	} else {
	    tipState->insert(pair<string, string>(path + "/" + (*it).first,
				    (*it).second.hash));
	}
    }

    return 0;
}

int
cmd_status(int argc, const char *argv[])
{
    map<string, string> dirState;
    map<string, string> tipState;
    map<string, string>::iterator it;
    Commit c;
    string tip = repository.getHead();

    if (tip != EMPTY_COMMIT) {
        c = repository.getCommit(tip);
	StatusTreeIter(&tipState, "", c.getTree());
    }

    Scan_RTraverse(Repo::getRootPath().c_str(),
		   (void *)&dirState,
	           StatusDirectoryCB);

    for (it = dirState.begin(); it != dirState.end(); it++) {
	map<string, string>::iterator k = tipState.find((*it).first);
	if (k == tipState.end()) {
	    printf("A	%s\n", (*it).first.c_str());
	} else if ((*k).second != (*it).second) {
	    // XXX: Handle replace a file <-> directory with same name
	    printf("M	%s\n", (*it).first.c_str());
	}
    }

    for (it = tipState.begin(); it != tipState.end(); it++) {
	map<string, string>::iterator k = dirState.find((*it).first);
	if (k == dirState.end()) {
	    printf("D	%s\n", (*it).first.c_str());
	}
    }
}

int
cmd_checkout(int argc, const char *argv[])
{
    map<string, string> dirState;
    map<string, string> tipState;
    map<string, string>::iterator it;
    Commit c;
    string tip = repository.getHead();

    if (argc == 2) {
	tip = argv[1];
    }

    if (tip != EMPTY_COMMIT) {
        c = repository.getCommit(tip);
	StatusTreeIter(&tipState, "", c.getTree());
    }

    Scan_RTraverse(Repo::getRootPath().c_str(),
		   (void *)&dirState,
	           StatusDirectoryCB);

    for (it = dirState.begin(); it != dirState.end(); it++) {
	map<string, string>::iterator k = tipState.find((*it).first);
	if (k == tipState.end()) {
	    printf("A	%s\n", (*it).first.c_str());
	} else if ((*k).second != (*it).second) {
	    printf("M	%s\n", (*it).first.c_str());
	    // XXX: Handle replace a file <-> directory with same name
	    assert((*it).second != "DIR");
	    repository.copyObject((*k).second,
				  Repo::getRootPath()+(*k).first);
	}
    }

    for (it = tipState.begin(); it != tipState.end(); it++) {
	map<string, string>::iterator k = dirState.find((*it).first);
	if (k == dirState.end()) {
	    printf("D	%s\n", (*it).first.c_str());
	    string path = Repo::getRootPath() + (*it).first;
	    if ((*it).second == "DIR") {
		mkdir(path.c_str(), 0755);
	    } else {
		repository.copyObject((*it).second, path);
	    }
	}
    }
}

int
cmd_log(int argc, const char *argv[])
{
    string commit = repository.getHead();

    while (commit != EMPTY_COMMIT) {
	Commit c = repository.getCommit(commit);

	printf("commit:  %s\n", commit.c_str());
	printf("parents: %s\n", c.getParents().first.c_str());
	printf("%s\n\n", c.getMessage().c_str());

	commit = c.getParents().first;
	// XXX: Handle merge cases
    }
}

int
cmd_clone(int argc, const char *argv[])
{
    string srcRoot;
    string newRoot;
    const char *initArgs[2] = { NULL, NULL };

    if (argc != 2 && argc != 3) {
	printf("Specify a repository to clone.\n");
	printf("usage: ori clone <repo> [<dir>]\n");
	return 1;
    }

    srcRoot = argv[1];
    if (argc == 2) {
	newRoot = srcRoot.substr(srcRoot.rfind("/")+1);
    } else {
	newRoot = argv[2];
    }
    initArgs[1] = newRoot.c_str();
    cmd_init(2, initArgs);

    printf("Cloning from %s to %s\n", srcRoot.c_str(), newRoot.c_str());

    Repo srcRepo(srcRoot);
    Repo dstRepo(newRoot);

    dstRepo.pull(&srcRepo);

    // XXX: Need to rely on sync log.
    dstRepo.updateHead(srcRepo.getHead());

    return 0;
}

int
cmd_pull(int argc, const char *argv[])
{
    string srcRoot;

    if (argc != 2) {
	printf("Specify a repository to pull.\n");
	printf("usage: ori pull <repo>\n");
	return 1;
    }

    srcRoot = argv[1];

    printf("Pulling from %s\n", srcRoot.c_str());

    Repo srcRepo(srcRoot);
    repository.pull(&srcRepo);

    // XXX: Need to rely on sync log.
    repository.updateHead(srcRepo.getHead());

    return 0;
}


