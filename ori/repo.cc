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
#include <errno.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <uuid.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "debug.h"
#include "scan.h"
#include "util.h"
#include "repo.h"

using namespace std;

Repo repository;

/********************************************************************
 *
 *
 * TreeEntry
 *
 *
 ********************************************************************/

TreeEntry::TreeEntry()
{
    mode = Null;
}

TreeEntry::~TreeEntry()
{
}

/********************************************************************
 *
 *
 * Tree
 *
 *
 ********************************************************************/

Tree::Tree()
{
}

Tree::~Tree()
{
}

void
Tree::addObject(const char *path, const string &objId)
{
    string fileName = path;
    struct stat sb;
    TreeEntry entry = TreeEntry();

    if (stat(path, &sb) < 0) {
	perror("stat failed in Tree::addObject");
	assert(false);
    }

    if (sb.st_mode & S_IFDIR) {
	entry.type = TreeEntry::Tree;
    } else {
	entry.type = TreeEntry::Blob;
    }
    entry.mode = sb.st_mode & (S_IRWXO | S_IRWXG | S_IRWXU | S_IFDIR);
    entry.hash = objId;

    fileName = fileName.substr(fileName.rfind("/") + 1);
    tree[fileName] = entry;
}


const string
Tree::getBlob()
{
    string blob = "";
    map<string, TreeEntry>::iterator it;

    for (it = tree.begin(); it != tree.end(); it++)
    {
	blob += (*it).second.type == TreeEntry::Tree ? "tree" : "blob";
	blob += " ";
	blob += (*it).second.hash;
	blob += " ";
	blob += (*it).first;
	blob += "\n";
    }

    return blob;
}

void
Tree::fromBlob(const string &blob)
{
    string line;
    stringstream ss(blob);

    while (getline(ss, line, '\n')) {
	TreeEntry entry = TreeEntry();
	if (line.substr(0, 5) == "tree ") {
	    entry.type = TreeEntry::Tree;
	} else if (line.substr(0, 5) == "blob ") {
	    entry.type = TreeEntry::Blob;
	} else {
	    assert(false);
	}
	// XXX: entry.mode
	entry.hash = line.substr(5, 64);
	tree[line.substr(70)] = entry;
    }
}

/********************************************************************
 *
 *
 * Commit
 *
 *
 ********************************************************************/

Commit::Commit()
{
}

Commit::~Commit()
{
}

void
Commit::setParents(std::string p1, std::string p2)
{
    parents.first = p1;
    parents.second = p2;
}

pair<string, string>
Commit::getParents()
{
    return parents;
}

void
Commit::setMessage(const string &msg)
{
    message = msg;
}

string
Commit::getMessage() const
{
    return message;
}

void
Commit::setTree(const string &tree)
{
    treeObjId = tree;
}

string
Commit::getTree() const
{
    return treeObjId;
}

const string
Commit::getBlob()
{
    string blob;
    
    blob = "tree " + treeObjId + "\n";
    blob += "parent " + parents.first;
    if (parents.second != "") {
	blob += " " + parents.second;
    }
    blob += "\n\n";
    blob += message;

    return blob;
}

void
Commit::fromBlob(const string &blob)
{
    string line;
    stringstream ss(blob);

    while (getline(ss, line, '\n')) {
	if (line.substr(0, 5) == "tree ") {
	    treeObjId = line.substr(5);
	} else if (line.substr(0, 7) == "parent ") {
	    // XXX: Handle the merge case
	    parents.first = line.substr(7);
	} else if (line.substr(0, 1) == "") {
	    message = blob.substr(ss.tellg());
	    break;
	} else{
	    printf("Unsupported commit parameter!\n");
	    assert(false);
	}
    }

    // Verify that everything is set!
}

/********************************************************************
 *
 *
 * Repository
 *
 *
 ********************************************************************/

Repo::Repo(const string &root)
{
    rootPath = (root == "") ? getRootPath() : root;
}

Repo::~Repo()
{
}

bool
Repo::open()
{
    // FileStream f;
    //char buf[37];

    if (rootPath.compare("") == 0)
        return false;
/*
    // Read UUID
    fd = open(getRootPath() + ORI_PATH_UUID, O_RDONLY);
    if (f.getLength() != 36)
        return false;
    memset(buf, 0, sizeof(buf));
    f.read(buf, 36);
    id = buf;
    f.close();

    // Read Version
    f.open(getRootPath() + ORI_PATH_VERSION, O_RDONLY);
    if (f.getLength() > 30)
        return false;
    memset(buf, 0, sizeof(buf));
    f.read(buf, f.getLength());
    version = buf;

    f.close();
*/

    return true;
}

void
Repo::close()
{
}

/*
 * Object Operations
 */

void
Repo::createObjDirs(const string &objId)
{
    string path = rootPath;

    assert(path != "");

    path += ORI_PATH_OBJS;
    path += objId.substr(0,2);

    mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

    path += "/";
    path += objId.substr(2,2);

    mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
}

string
Repo::objIdToPath(const string &objId)
{
    string rval = rootPath;

    assert(rval != "");

    rval += ORI_PATH_OBJS;
    rval += objId.substr(0,2);
    rval += "/";
    rval += objId.substr(2,2);
    rval += "/";
    rval += objId;

    return rval;
}

/*
 * Add a file to the repository. This is a low-level interface.
 */
string
Repo::addFile(const string &path)
{
    string hash = Util_HashFile(path);
    string objPath;

    if (hash == "") {
	perror("Unable to hash file");
	return 0;
    }
    objPath = objIdToPath(hash);

    // Check if in tree
    if (!Util_FileExists(objPath)) {
	// Copy to object tree
	createObjDirs(hash);
	if (Util_CopyFile(path, objPath) < 0) {
	    perror("Unable to copy file");
	    return "";
	}
    }

    return hash;
}

/*
 * Add a blob to the repository. This is a low-level interface.
 */
string
Repo::addBlob(const string &blob)
{
    string hash = Util_HashString(blob);
    string objPath = objIdToPath(hash);

    // Check if in tree
    if (!Util_FileExists(objPath)) {
	// Copy to object tree
	createObjDirs(hash);
	if (Util_WriteFile(blob.c_str(), blob.length(), objPath) < 0) {
	    perror("Unable to write blob to disk");
	    return "";
	}
    }

    return hash;
}

/*
 * Add a tree to the repository.
 */
string
Repo::addTree(/* const */ Tree &tree)
{
    return addBlob(tree.getBlob());
}

/*
 * Add a commit to the repository.
 */
string
Repo::addCommit(/* const */ Commit &commit)
{
    string blob = commit.getBlob();

    return addBlob(blob);
}

/*
 * Read an object into memory and return it as a string.
 */
char *
Repo::getObject(const string &objId)
{
    string path = objIdToPath(objId);

    return Util_ReadFile(path, NULL);
}

/*
 * Get an object length.
 */
size_t
Repo::getObjectLength(const char *objId)
{
    string objPath = objIdToPath(objId);
    struct stat sb;

    if (stat(objPath.c_str(), &sb) < 0) {
	perror("Cannot get object length");
	return -1;
    }

    return sb.st_size;
}

/*
 * Copy an object to a working directory.
 */
bool
Repo::copyObject(const char *objId, const char *path)
{
    string objPath = objIdToPath(objId);

    if (Util_CopyFile(objPath, path) < 0)
	return false;
    return true;
}

int
Repo_GetObjectsCB(void *arg, const char *path)
{
    string hash;
    set<string> *objs = (set<string> *)arg;

    // XXX: Only add the hash
    if (Util_IsDirectory(path)) {
	return 0;
    }

    hash = path;
    hash = hash.substr(hash.rfind("/") + 1);
    objs->insert(hash);

    return 0;
}

/*
 * Return a list of objects in the repository.
 */
set<string>
Repo::getObjects()
{
    int status;
    set<string> objs;
    string objRoot = rootPath + ORI_PATH_OBJS;

    status = Scan_RTraverse(objRoot.c_str(), (void *)&objs, Repo_GetObjectsCB);
    if (status < 0)
	perror("getObjects");

    return objs;
}

/*
 * Working Directory Operations
 */

/*
 * Get the working repository version.
 */
string
Repo::getHead()
{
    string headPath = rootPath + ORI_PATH_HEAD;
    char *commitId = Util_ReadFile(headPath, NULL);
    // XXX: Leak!

    if (commitId == NULL) {
	return EMPTY_COMMIT;
    }

    return commitId;
}

/*
 * Update the working directory revision.
 */
void
Repo::updateHead(const string &commitId)
{
    string headPath = rootPath + ORI_PATH_HEAD;

    Util_WriteFile(commitId.c_str(), commitId.size(), headPath);
}

/*
 * General Operations
 */

string
Repo::getUUID()
{
    return id;
}

string
Repo::getVersion()
{
    return version;
}

string
Repo::getRootPath()
{
    char *cwd = getcwd(NULL, MAXPATHLEN);
    string root;
    string uuidfile;
    struct stat dbstat;

    if (cwd == NULL) {
        perror("getcwd");
        exit(1);
    }
    root = cwd;
    free(cwd);

    // look for the UUID file
    while (1) {
        uuidfile = root + ORI_PATH_UUID;
        if (stat(uuidfile.c_str(), &dbstat) == 0)
            return root;

        size_t slash = root.rfind('/');
        if (slash == 0)
            return "";
        root.erase(slash);
    }

    return root;
}

string
Repo::getLogPath()
{
    string rootPath = Repo::getRootPath();
    
    if (rootPath.compare("") == 0)
        return rootPath;
    return rootPath + ORI_PATH_LOG;
}

string
Repo::getTmpFile()
{
    string rootPath = Repo::getRootPath();
    string tmpFile;
    char buf[10];
    // Declare static as an optimization 
    static int id = 1;
    struct stat fileStat;

    if (rootPath == "")
        return rootPath;

    while (1) {
        snprintf(buf, 10, "%08x", id++);
        tmpFile = rootPath + ORI_PATH_TMP + "tmp" + buf;

        if (stat(tmpFile.c_str(), &fileStat) == 0)
            continue;
        if (errno != ENOENT)
            break;
    }

    return tmpFile;
}

