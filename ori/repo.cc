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
#include <set>
#include <queue>
#include <iostream>

#include "debug.h"
#include "scan.h"
#include "util.h"
#include "object.h"
#include "largeblob.h"

#include "repo.h"
#include "localrepo.h"

using namespace std;


/*
 * Repo
 */

int Repo::addObject(const ObjectInfo &info,
        const std::string &payload)
{
    return -1;
}

int Repo::getData(ObjectInfo *info,
        std::string &data) {
    return -1;
}


LocalRepo repository;

/********************************************************************
 *
 *
 * LocalRepo
 *
 *
 ********************************************************************/

LocalRepo::LocalRepo(const string &root)
{
    rootPath = (root == "") ? findRootPath() : root;
}

LocalRepo::~LocalRepo()
{
}

bool
LocalRepo::open(const string &root)
{
    if (root.compare("") != 0) {
        rootPath = root;
    }

    if (rootPath.compare("") == 0)
        return false;

    // Read UUID
    std::string uuid_path = rootPath + ORI_PATH_UUID;
    char *id_str = Util_ReadFile(uuid_path, NULL);
    if (id_str == NULL)
        return false;
    id = id_str;

    // Read Version
    char *ver_str = Util_ReadFile(rootPath + ORI_PATH_VERSION, NULL);
    if (ver_str == NULL)
        return false;
    version = ver_str;

    return true;
}

void
LocalRepo::close()
{
}

/*
 * Object Operations
 */

int LocalRepo::getObjectInfo(ObjectInfo *info)
{
    string path = objIdToPath(info->hash);
    Object o;
    o.open(path);
    *info = o.getInfo();

    return 0;
}

void
LocalRepo::createObjDirs(const string &objId)
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
LocalRepo::objIdToPath(const string &objId)
{
    string rval = rootPath;

    assert(objId.length() == 64);
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
LocalRepo::addSmallFile(const string &path)
{
    string hash = Util_HashFile(path);
    string objPath;

    if (hash == "") {
	perror("Unable to hash file");
	return "";
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
 * Add a file to the repository. This is a low-level interface.
 */
string
LocalRepo::addLargeFile(const string &path)
{
    string blob;
    string hash;
    LargeBlob lb = LargeBlob(this);

    lb.chunkFile(path);
    blob = lb.getBlob();
    hash = Util_HashString(blob);

    if (!hasObject(hash)) {
        map<uint64_t, LBlobEntry>::iterator it;

        for (it = lb.parts.begin(); it != lb.parts.end(); it++) {
            Object o = Object();
            string refPath = objIdToPath((*it).second.hash);

            if (o.open(refPath) != 0) {
                perror("Cannot open object");
                assert(false);
            }
            o.addBackref(hash, Object::BRRef);
            o.close();
        }
    }

    return addBlob(Object::LargeBlob, blob);
}

/*
 * Add a file to the repository. This is an internal interface that pusheds the
 * work to addLargeFile or addSmallFile based on our size threshold.
 */
string
LocalRepo::addFile(const string &path)
{
    size_t sz = Util_FileSize(path);

    if (sz > LARGEFILE_MINIMUM)
        return addLargeFile(path);
    else
        return addSmallFile(path);
}

/*
 * Add a blob to the repository. This is a low-level interface.
 */
string
Repo::addBlob(Object::Type type, const string &blob)
{
    string hash = Util_HashString(blob);
    ObjectInfo info(hash.c_str());
    info.type = type;
    //info.flags = ...;
    addObject(info, blob);
    return hash;
}

int
LocalRepo::addObject(const ObjectInfo &info, const string &payload)
{
    string objPath = objIdToPath(info.hash);

    // Check if in tree
    if (!Util_FileExists(objPath)) {
        // Copy to object tree
	Object o = Object();

	createObjDirs(info.hash);
	if (o.create(objPath, info.type) < 0) {
	    perror("Unable to create object");
	    return -errno;
	}
	if (o.appendBlob(payload) < 0) {
	    perror("Unable to copy file");
	    return -errno;
	}
    }

    return 0;
}

/*
 * Add a tree to the repository.
 */
string
LocalRepo::addTree(const Tree &tree)
{
    string blob = tree.getBlob();
    string hash = Util_HashString(blob);
    map<string, TreeEntry>::const_iterator it;

    if (hasObject(hash)) {
        return hash;
    }

    for (it = tree.tree.begin(); it != tree.tree.end(); it++) {
        Object o = Object();
        string refPath = objIdToPath((*it).second.hash);

        if (o.open(refPath) != 0) {
            perror("Cannot open object");
            assert(false);
        }
        o.addBackref(hash, Object::BRRef);
        o.close();
    }

    return addBlob(Object::Tree, blob);
}

/*
 * Add a commit to the repository.
 */
string
LocalRepo::addCommit(/* const */ Commit &commit)
{
    string blob = commit.getBlob();
    string hash = Util_HashString(blob);
    Object o = Object();
    string refPath;

    if (hasObject(hash)) {
        return hash;
    }

    refPath = objIdToPath(commit.getTree());
    if (o.open(refPath) != 0) {
        perror("Cannot open object");
    }
    o.addBackref(hash, Object::BRRef);
    o.close();

    refPath = commit.getParents().first;
    if (refPath != EMPTY_COMMIT) {
        refPath = objIdToPath(refPath);
        if (o.open(refPath) != 0) {
            cout << refPath << endl;;
            perror("Cannot open object");
        }
        o.addBackref(hash, Object::BRRef);
        o.close();
    }

    refPath = commit.getParents().second;
    if (refPath != "") {
        refPath = objIdToPath(refPath);
        if (o.open(refPath) != 0) {
            perror("Cannot open object");
        }
        o.addBackref(hash, Object::BRRef);
        o.close();
    }

    return addBlob(Object::Commit, blob);
}

/*
 * Get the Object associated with an ID.
 */
Object
LocalRepo::getObject(const string &objId)
{
    string path = objIdToPath(objId);
    Object o;

    // XXX: Add better error handling
    if (o.open(path) < 0)
	return o;

    return o;
}

/*
 * Read an object into memory and return it as a string.
 */
std::string
LocalRepo::getPayload(const string &objId)
{
    Object o = getObject(objId);
    // XXX: Add better error handling
    if (o.getInfo().type == Object::Null)
        return "";
    return o.extractBlob();
}

/*
 * Get an object length.
 */
size_t
Repo::getObjectLength(const string &objId)
{
    ObjectInfo info(objId.c_str());

    // XXX: Add better error handling
    if (getObjectInfo(&info) < 0)
	return 0;

    return info.payload_size;
}

/*
 * Verify object
 */
string
LocalRepo::verifyObject(const string &objId)
{
    string objPath = objIdToPath(objId);
    Object o = Object();
    Object::Type type;

    if (!hasObject(objId))
	return "Object not found!";

    // XXX: Add better error handling
    if (o.open(objPath) < 0)
	return "Cannot open object!";

    type = o.getType();
    switch(type) {
	case Object::Null:
	    return "Object with Null type!";
	case Object::Commit:
	{
	    if (o.computeHash() != objId)
		return "Object hash mismatch!"; 

	    // XXX: Verify tree and parents exist
	    break;
	}
	case Object::Tree:
	{
	    if (o.computeHash() != objId)
		return "Object hash mismatch!"; 

	    // XXX: Verify subtrees and blobs exist
	    break;
	}
	case Object::Blob:
	{
	    if (o.computeHash() != objId)
		return "Object hash mismatch!"; 

	    break;
	}
        case Object::LargeBlob:
        {
            // XXX: Verify fragments
            break;
        }
	case Object::Purged:
	    break;
	default:
	    return "Object with unknown type!";
    }

    // Check object metadata
    if (!o.checkMetadata()) {
        return "Object metadata hash mismatch!";
    }

    return "";
}

/*
 * Purge object
 */
bool
LocalRepo::purgeObject(const string &objId)
{
    string objPath = objIdToPath(objId);
    Object o = Object();

    if (o.open(objPath) < 0)
	return false;

    if (o.purge() < 0)
	return false;

    return true;
}

/*
 * Get the object type.
 */
Object::Type
Repo::getObjectType(const string &objId)
{
    ObjectInfo info(objId.c_str());

    // XXX: Add better error handling
    if (getObjectInfo(&info) < 0)
	return Object::Null;

    return info.type;
}

/*
 * Copy an object to a working directory.
 */
bool
LocalRepo::copyObject(const string &objId, const string &path)
{
    string objPath = objIdToPath(objId);
    Object o = Object();

    // XXX: Add better error handling
    if (o.open(objPath) < 0)
	return false;

    if (o.getType() == Object::Blob) {
        if (o.extractFile(path) < 0)
	    return false;
    } else if (o.getType() == Object::LargeBlob) {
        LargeBlob lb = LargeBlob(this);
        lb.fromBlob(o.extractBlob());
        lb.extractFile(path);
    }
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
LocalRepo::listObjects()
{
    int status;
    set<string> objs;
    string objRoot = rootPath + ORI_PATH_OBJS;

    status = Scan_RTraverse(objRoot.c_str(), (void *)&objs, Repo_GetObjectsCB);
    if (status < 0)
	perror("listObjects");

    return objs;
}

Commit
LocalRepo::getCommit(const std::string &commitId)
{
    Commit c = Commit();
    string blob = getPayload(commitId);
    c.fromBlob(blob);

    return c;
}

Tree
Repo::getTree(const std::string &treeId)
{
    ObjectInfo info(treeId.c_str());
    string blob;
    getData(&info, blob);

    assert(info.type == Object::Tree);

    Tree t;
    t.fromBlob(blob);

    return t;
}

/*
 * Return true if the repository has the object.
 */
bool
LocalRepo::hasObject(const string &objId)
{
    string path = objIdToPath(objId);

    return Util_FileExists(path);
}



/*
 * BasicRepo implementation
 */
int
LocalRepo::getDataRaw(ObjectInfo *info, std::string &raw_data)
{
    return -1;
}

Object
LocalRepo::addObjectRaw(const ObjectInfo &info, const std::string &raw_data)
{
    string hash = info.hash;
    string objPath = objIdToPath(hash);

    Object o;

    if (!Util_FileExists(objPath)) {
        createObjDirs(hash);
        if (o.createFromRawData(objPath, info, raw_data) < 0) {
            perror("Unable to create object");
        }
    }

    return o;
}

/*
 * Reference Counting Operations
 */

/*
 * Return the references for a given object.
 */
map<string, Object::BRState>
LocalRepo::getRefs(const string &objId)
{
    string objPath = objIdToPath(objId);
    Object o = Object();
    map<string, Object::BRState> rval;

    if (o.open(objPath) != 0)
        return rval;
    rval = o.getBackref();
    o.close();

    return rval;
}

/*
 * Return reference counts for all objects.
 */
map<string, map<string, Object::BRState> >
LocalRepo::getRefCounts()
{
    set<string> obj = listObjects();
    set<string>::iterator it;
    map<string, map<string, Object::BRState> > rval;

    for (it = obj.begin(); it != obj.end(); it++) {
        rval[*it] = getRefs(*it);
    }

    return rval;
}

/*
 * Construct a raw set of references. This is the slow path and should only
 * be used as part of recovery.
 */
map<string, set<string> >
LocalRepo::computeRefCounts()
{
    set<string> obj = listObjects();
    set<string>::iterator it;
    map<string, set<string> > rval;
    map<string, set<string> >::iterator key;

    for (it = obj.begin(); it != obj.end(); it++) {
        key = rval.find(*it);
        if (key == rval.end())
            rval[*it] = set<string>();
        switch (getObjectType(*it)) {
            case Object::Commit:
            {
                Commit c = getCommit(*it);
                
                key = rval.find(c.getTree());
                if (key == rval.end())
                    rval[c.getTree()] = set<string>();
                rval[c.getTree()].insert(*it);
                
                key = rval.find(c.getParents().first);
                if (key == rval.end())
                    rval[c.getParents().first] = set<string>();
                rval[c.getParents().first].insert(*it);
                
                if (c.getParents().second != "") {
                    key = rval.find(c.getParents().second);
                    if (key == rval.end())
                        rval[c.getTree()] = set<string>();
                    rval[c.getParents().second].insert(*it);
                }
                
                break;
            }
            case Object::Tree:
            {
                Tree t = getTree(*it);
                map<string, TreeEntry>::iterator tt;

                for  (tt = t.tree.begin(); tt != t.tree.end(); tt++) {
                    string h = (*tt).second.hash;
                    key = rval.find(h);
                    if (key == rval.end())
                        rval[h] = set<string>();
                    rval[h].insert(*it);
                }
                break;
            }
            case Object::Blob:
	    case Object::Purged:
                break;
            default:
                cout << "Unsupported object type!" << endl;
                assert(false);
                break;
        }
    }

    return rval;
}

/*
 * Grafting Operations
 */

/*
 * Return a set of all objects references by a tree.
 */
set<string>
LocalRepo::getSubtreeObjects(const string &treeId)
{
    queue<string> treeQ;
    set<string> rval;

    treeQ.push(treeId);

    while (!treeQ.empty()) {
	map<string, TreeEntry>::iterator it;
	Tree t = getTree(treeQ.front());
	treeQ.pop();

	for (it = t.tree.begin(); it != t.tree.end(); it++) {
	    TreeEntry e = (*it).second;
	    set<string>::iterator p = rval.find((*it).first);

	    if (p != rval.end()) {
		if (e.type == TreeEntry::Tree) {
		    treeQ.push(e.hash);
		}
		rval.insert(e.hash);
	    }
	}
    }

    return rval;
}

/*
 * Walk the repository history.
 * XXX: Make this a template function
 */
set<string>
LocalRepo::walkHistory(HistoryCB &cb)
{
    set<string> rval;
    set<string> curLevel;
    set<string> nextLevel;
    set<string>::iterator it;

    if (getHead() == EMPTY_COMMIT)
        return rval;

    nextLevel.insert(getHead());

    while (!nextLevel.empty()) {
	curLevel = nextLevel;
	nextLevel.clear();

	for (it = curLevel.begin(); it != curLevel.end(); it++) {
	    Commit c = getCommit(*it);
	    pair<string, string> p = c.getParents();
            string val;

	    val = cb.cb(*it, &c);
            if (val != "")
                rval.insert(val);

            if (p.first != EMPTY_COMMIT) {
	        nextLevel.insert(p.first);
	        if (p.second != "")
		    nextLevel.insert(p.second);
            }
	}
    }

    return rval;
}

/*
 * Lookup a path given a Commit and return the object ID.
 */
string
LocalRepo::lookup(const Commit &c, const string &path)
{
    vector<string> pv = Util_PathToVector(path);
    vector<string>::iterator it;
    string objId = c.getTree();

    for (it = pv.begin(); it != pv.end(); it++) {
        Tree t = getTree(objId);
        objId = t.tree[*it].hash;
    }

    return objId;
}

class GraftCB : public HistoryCB
{
public:
    GraftCB(LocalRepo *dstRepo, LocalRepo *srcRepo, const string &path)
    {
        dst = dstRepo;
        src = srcRepo;
        srcPath = path;
    }
    virtual ~GraftCB()
    {
    }
    virtual string cb(const string &commitId, Commit *c)
    {
        string treeId = src->lookup(*c, srcPath);
        set<string> objects;
        set<string>::iterator it;

        if (treeId == "")
            return "";

        cout << "Commit: " << commitId << endl;
        cout << "Tree: " << treeId << endl;

        // Copy objects
        objects = src->getSubtreeObjects(treeId);
        for (it = objects.begin(); it != objects.end(); it++)
        {
	    if (!dst->hasObject(*it)) {
                // XXX: Copy object without loading it all into memory!
	        dst->addBlob(src->getObjectType(*it), src->getPayload(*it));
	    }
        }

        // XXX: Create merge commit

        // XXX: Copy files and mark in working state.

        return "";
    }
private:
    string srcPath;
    LocalRepo *src;
    LocalRepo *dst;
};

/*
 * Graft a subtree from Repo 'r' to this repository.
 */
string
LocalRepo::graftSubtree(LocalRepo *r,
                   const std::string &srcPath,
                   const std::string &dstPath)
{
    GraftCB cb = GraftCB(this, r, srcPath);
    set<string> changes;

    cout << "Hello!" << endl;
    changes = r->walkHistory(cb);

    // XXX: TODO
    NOT_IMPLEMENTED(false);
}

/*
 * Working Directory Operations
 */

/*
 * Get the working repository version.
 */
string
LocalRepo::getHead()
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
LocalRepo::updateHead(const string &commitId)
{
    string headPath = rootPath + ORI_PATH_HEAD;

    Util_WriteFile(commitId.c_str(), commitId.size(), headPath);
}

/*
 * General Operations
 */

string
LocalRepo::getUUID()
{
    return id;
}

string
LocalRepo::getVersion()
{
    return version;
}

string
LocalRepo::getRootPath()
{
    return rootPath;
}

string
LocalRepo::getLogPath()
{
    if (rootPath.compare("") == 0)
        return rootPath;
    return rootPath + ORI_PATH_LOG;
}

string
LocalRepo::getTmpFile()
{
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


/*
 * High Level Operations
 */

/*
 * Pull changes from the source repository.
 */
void
Repo::pull(Repo *r)
{
    set<string> objects = r->listObjects();
    set<string>::iterator it;

    for (it = objects.begin(); it != objects.end(); it++)
    {
	if (!hasObject(*it)) {
	    // XXX: Copy object without loading it all into memory!
	    //addBlob(r->getPayload(*it), r->getObjectType(*it));
            Object::ObjectInfo info((*it).c_str());
            std::string raw_data;
            if (r->getDataRaw(&info, raw_data) < 0) {
                printf("Error getting object %s\n", (*it).c_str());
                continue;
            }

            addObjectRaw(info, raw_data);
	}
    }
}

/*
 * Static Operations
 */

string
LocalRepo::findRootPath(const string &path)
{
    string root = path;
    if (path.size() == 0) {
        char *cwd = getcwd(NULL, MAXPATHLEN);
        if (cwd == NULL) {
            perror("getcwd");
            exit(1);
        }

        root.assign(cwd);
        free(cwd);
    }

    string uuidfile;
    struct stat dbstat;

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

