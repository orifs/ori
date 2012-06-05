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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
//#include <uuid.h>

#include <string>

#include "debug.h"
#include "scan.h"
#include "util.h"
#include "object.h"
#include "repo.h"

using namespace std;

Repo repository;

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
	Object o = Object();

	createObjDirs(hash);
	if (o.create(objPath, Object::Blob) < 0) {
	    perror("Unable to create object");
	    return "";
	}
	if (o.appendFile(path) < 0) {
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
Repo::addBlob(const string &blob, Object::Type type)
{
    string hash = Util_HashString(blob);
    string objPath = objIdToPath(hash);

    // Check if in tree
    if (!Util_FileExists(objPath)) {
	// Copy to object tree
	Object o = Object();

	createObjDirs(hash);
	if (o.create(objPath, type) < 0) {
	    perror("Unable to create object");
	    return "";
	}
	if (o.appendBlob(blob) < 0) {
	    perror("Unable to copy file");
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
    return addBlob(tree.getBlob(), Object::Tree);
}

/*
 * Add a commit to the repository.
 */
string
Repo::addCommit(/* const */ Commit &commit)
{
    string blob = commit.getBlob();

    return addBlob(blob, Object::Commit);
}

/*
 * Read an object into memory and return it as a string.
 */
std::string
Repo::getObject(const string &objId)
{
    string path = objIdToPath(objId);
    Object o = Object();

    // XXX: Add better error handling
    if (o.open(path) < 0)
	return "";

    return o.extractBlob();
}

/*
 * Get an object length.
 */
size_t
Repo::getObjectLength(const string &objId)
{
    string objPath = objIdToPath(objId);
    Object o = Object();

    // XXX: Add better error handling
    if (o.open(objPath) < 0)
	return false;

    return o.getObjectSize();
}

/*
 * Verify object
 */
string
Repo::verifyObject(const string &objId)
{
    string objPath = objIdToPath(objId);
    Object o = Object();
    Object::Type type;

    if (!hasObject(objId))
	return "Object not found!";

    // XXX: Add better error handling
    if (o.open(objPath) < 0)
	return "Cannot open object!";

    if (o.computeHash() != objId)
	return "Object hash mismatch!"; 

    type = o.getType();
    switch(type) {
	case Object::Null:
	    return "Object with Null type!";
	case Object::Commit:
	    // XXX: Verify tree and parents exist
	    break;
	case Object::Tree:
	    // XXX: Verify subtrees and blobs exist
	case Object::Blob:
	    break;
	default:
	    return "Object with unknown type!";
    }

    return "";
}

/*
 * Get the object type.
 */
Object::Type
Repo::getObjectType(const string &objId)
{
    string objPath = objIdToPath(objId);
    Object o = Object();

    // XXX: Add better error handling
    if (o.open(objPath) < 0)
	return Object::Null;

    return o.getType();
}

/*
 * Copy an object to a working directory.
 */
bool
Repo::copyObject(const string &objId, const string &path)
{
    string objPath = objIdToPath(objId);
    Object o = Object();

    // XXX: Add better error handling
    if (o.open(objPath) < 0)
	return false;

    if (o.extractFile(path) < 0)
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

Commit
Repo::getCommit(const std::string &commitId)
{
    Commit c = Commit();
    string blob = getObject(commitId);
    c.fromBlob(blob);

    return c;
}

Tree
Repo::getTree(const std::string &treeId)
{
    Tree t = Tree();
    string blob = getObject(treeId);
    t.fromBlob(blob);

    return t;
}

/*
 * Return true if the repository has the object.
 */
bool
Repo::hasObject(const string &objId)
{
    string path = objIdToPath(objId);

    return Util_FileExists(path);
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

/*
 * High Level Operations
 */

/*
 * Pull changes from the source repository.
 */
void
Repo::pull(Repo *r)
{
    set<string> objects = r->getObjects();
    set<string>::iterator it;

    for (it = objects.begin(); it != objects.end(); it++)
    {
	if (!hasObject(*it)) {
	    addBlob(r->getObject(*it), r->getObjectType(*it));
	}
    }
}

/*
 * Static Operations
 */

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

