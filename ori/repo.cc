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
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
//#include <uuid.h>

#include <string>

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
    tree[path] = entry;
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
    if (root == "")
	rootPath = getRootPath();
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
    string hash = Util_HashFile(path.c_str());
    string objPath;

    if (hash == "") {
	perror("Unable to hash file");
	return 0;
    }
    objPath = objIdToPath(hash);

    // Check if in tree
    if (!Util_FileExists(objPath.c_str())) {
	// Copy to object tree
	createObjDirs(hash);
	if (Util_CopyFile(path.c_str(), objPath.c_str()) < 0) {
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
    if (!Util_FileExists(objPath.c_str())) {
	// Copy to object tree
	createObjDirs(hash);
	if (Util_WriteFile(blob.c_str(), blob.length(), objPath.c_str()) < 0) {
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

    return Util_ReadFile(path.c_str(), NULL);
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

    if (Util_CopyFile(objPath.c_str(), path) < 0)
	return false;
    return true;
}

int
Repo_GetObjectsCB(void *arg, const char *path)
{
    set<string> *objs = (set<string> *)arg;

    // XXX: Only add the hash

    objs->insert(path);
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


/********************************************************************
 *
 *
 * Commands
 *
 *
 ********************************************************************/


int
cmd_init(int argc, char *argv[])
{
    string rootPath;
    string oriDir;
    string tmpDir;
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
cmd_show(int argc, char *argv[])
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
cmd_catobj(int argc, char *argv[])
{
    size_t len;
    char *buf;
    bool hex = false;

    len = repository.getObjectLength(argv[1]);
    if (len < 0) {
	printf("Object does not exist.\n");
	return 1;
    }

    buf = repository.getObject(argv[1]);

    for (int i = 0; i < 0; i++) {
	if (buf[i] < 1 || buf[i] >= 0x80) {
	    hex = true;
	    break;
	}
    }

    if (hex) {
	// XXX: Add hex pretty printer
	printf("hex object!\n");
    } else {
	printf("%s", buf);
    }

    return 0;
}

int
commitHelper(void *arg, const char *path)
{
    string hash;
    Tree *tree = (Tree *)arg;

    if (Util_IsDirectory(path)) {
	string blob;
	Tree subTree = Tree();

	Scan_Traverse(path, &subTree, commitHelper);

	blob = subTree.getBlob();
	hash = repository.addBlob(blob);
    } else {
	hash = repository.addFile(path);
    }
    tree->addObject(path, hash);

    return 0;
}

int
cmd_commit(int argc, char *argv[])
{
    string blob;
    string treeHash, commitHash;
    Tree tree = Tree();
    Commit commit = Commit();
    string root = Repo::getRootPath();

    Scan_Traverse(root.c_str(), &tree, commitHelper);

    blob = tree.getBlob();
    treeHash = repository.addBlob(blob);

    // XXX: Get parents
    commit.setParents("1234");
    commit.setMessage("");
    commit.setTree(treeHash);

    commitHash = repository.addCommit(commit);

    // XXX: Update .ori/tip

    printf("Commit Hash: %s\nTree Hash: %s\n%s",
	   commitHash.c_str(),
	   treeHash.c_str(),
	   blob.c_str());
}

int
cmd_checkout(int argc, char *argv[])
{
}

int
cmd_status(int argc, char *argv[])
{
}

int
cmd_log(int argc, char *argv[])
{
}

