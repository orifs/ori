#include <cassert>
#include <stdbool.h>
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>

#include <string>
#include <deque>
#include <queue>
#include <set>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>

using namespace std;

#include "localrepo.h"
#include "largeblob.h"
#include "util.h"
#include "scan.h"
#include "debug.h"
#include "sshrepo.h"

int
LocalRepo_Init(const std::string &rootPath)
{
    string oriDir;
    string tmpDir;
    string objDir;
    string versionFile;
    string uuidFile;
    int fd;

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

    // Create refs directory
    tmpDir = rootPath + ORI_PATH_DIR + "/refs";
    if (mkdir(tmpDir.c_str(), 0755) < 0) {
        perror("Could not create '.ori/refs' directory");
        return 1;
    }

    // Create refs/heads directory
    tmpDir = rootPath + ORI_PATH_DIR + "/refs/heads";
    if (mkdir(tmpDir.c_str(), 0755) < 0) {
        perror("Could not create '.ori/refs/heads' directory");
        return 1;
    }

    // Create default branch
    tmpDir = rootPath + ORI_PATH_HEADS + "/default";
    if (!Util_WriteFile(EMPTY_COMMIT.hex().data(), ObjectHash::SIZE * 2, tmpDir))
    {
	perror("Could not create default branch");
	return 1;
    }

    // Set branch name
    if (!Util_WriteFile("default", 7, rootPath + ORI_PATH_BRANCH)) {
	perror("Could not create branch file");
	return 1;
    }

    // Create refs/remotes directory
    tmpDir = rootPath + ORI_PATH_DIR + "/refs/remotes";
    if (mkdir(tmpDir.c_str(), 0755) < 0) {
        perror("Could not create '.ori/refs/remotes' directory");
        return 1;
    }

    // Create first level of object sub-directories
    for (int i = 0; i < 256; i++)
    {
	stringstream hexval;
	string path = rootPath + ORI_PATH_OBJS;

	hexval << hex << setw(2) << setfill('0') << i;
	path += hexval.str();

	mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    }

    // Construct UUID
    uuidFile = rootPath + ORI_PATH_UUID;
    fd = open(uuidFile.c_str(), O_CREAT|O_WRONLY|O_APPEND, 0660);
    if (fd < 0) {
        perror("Could not create UUID file");
        return 1;
    }

    std::string generated_uuid = Util_NewUUID();
    write(fd, generated_uuid.data(), generated_uuid.length());
    close(fd);
    chmod(uuidFile.c_str(), 0440);

    // Construct version file
    versionFile = rootPath + ORI_PATH_VERSION;
    fd = open(versionFile.c_str(), O_CREAT|O_WRONLY|O_APPEND, 0660);
    if (fd < 0) {
        perror("Could not create version file");
        return 1;
    }
    write(fd, "ORI1.0", 6);
    close(fd);

    return 0;
}

/*
 * LocalRepoLock
 */
LocalRepoLock::LocalRepoLock(const std::string &filename)
    : lockFile(filename)
{
}

LocalRepoLock::~LocalRepoLock()
{
    if (lockFile.size() > 0) {
        if (Util_DeleteFile(lockFile) < 0) {
            perror("unlink");
        }
    }
}

/********************************************************************
 *
 *
 * LocalRepo
 *
 *
 ********************************************************************/

LocalRepo::LocalRepo(const string &root)
    : opened(false),
      remoteRepo(NULL)
{
    rootPath = (root == "") ? findRootPath() : root;
}

LocalRepo::~LocalRepo()
{
    close();
}

int
LocalRepo_PeerHelper(void *arg, const char *path)
{
    LocalRepo *l = (LocalRepo *)arg;
    Peer p = Peer(path);

    string name = path;
    name = name.substr(name.find_last_of("/") + 1);
    l->peers[name] = p;

    return 0;
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
    delete id_str;

    // Read Version
    char *ver_str = Util_ReadFile(rootPath + ORI_PATH_VERSION, NULL);
    if (ver_str == NULL)
        return false;
    version = ver_str;
    delete ver_str;

    index.open(rootPath + ORI_PATH_INDEX);
    snapshots.open(rootPath + ORI_PATH_SNAPSHOTS);
    if (!metadata.open(rootPath + ORI_PATH_METADATA))
        return false;

    // Scan for peers
    string peer_path = rootPath + ORI_PATH_REMOTES;
    Scan_Traverse(peer_path.c_str(), this, LocalRepo_PeerHelper);

    opened = true;
    return true;
}

void
LocalRepo::close()
{
    index.close();
    snapshots.close();
    opened = false;
}

LocalRepoLock *
LocalRepo::lock()
{
    assert(opened);
    if (rootPath == "")
        return NULL;
    std::string lfPath = rootPath + ORI_PATH_LOCK;
    std::string idPath = rootPath + ORI_PATH_UUID;

    int rval = symlink(idPath.c_str(), lfPath.c_str());
    if (rval < 0) {
        if (errno == EEXIST) {
            printf("Repository at %s is already locked\nAnother instance of ORI may currently be using it\n", rootPath.c_str());
        } else {
            perror("symlink");
        }

        exit(1);
    }

    return new LocalRepoLock(lfPath);
}

/*
 * Remote Operations
 */
void
LocalRepo::setRemote(Repo *r)
{
    remoteRepo = r;
}

void
LocalRepo::clearRemote()
{
    remoteRepo = NULL;
}

bool
LocalRepo::hasRemote()
{
    return (remoteRepo != NULL);
}

/*
 * Object Operations
 */

Object::sp LocalRepo::getObject(const ObjectHash &objId)
{
    LocalObject::sp o(getLocalObject(objId));

    if (!o) {
	if (remoteRepo != NULL)
	    // XXX: Save object locally
	    return remoteRepo->getObject(objId);
	else
	    return Object::sp();
    }

    return Object::sp(o);
}

LocalObject::sp LocalRepo::getLocalObject(const ObjectHash &objId)
{
    assert(opened);
    if (!_objectCache.hasKey(objId)) {
        string path = objIdToPath(objId);
        LocalObject::sp o(new LocalObject());

        int st = o->open(path, objId);
        if (st < 0) {
            LOG("LocalRepo couldn't open object %s: %s", objId.hex().c_str(),
                    strerror(st));
            return LocalObject::sp(); // NULL
        }

        _objectCache.put(objId, o);
    }
    return _objectCache.get(objId);
}

void
LocalRepo::createObjDirs(const ObjectHash &objId)
{
    string path = rootPath;
    string hexId = objId.hex();

    assert(path != "");

    path += ORI_PATH_OBJS;
    path += hexId.substr(0,2);

    //mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

    path += "/";
    path += hexId.substr(2,2);

    mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
}

string
LocalRepo::objIdToPath(const ObjectHash &objId)
{
    string rval = rootPath;
    string hexId = objId.hex();

    assert(!objId.isEmpty());
    assert(hexId.length() == 64);
    assert(rval != "");

    rval += ORI_PATH_OBJS;
    rval += hexId.substr(0,2);
    rval += "/";
    rval += hexId.substr(2,2);
    rval += "/";
    rval += hexId;

    return rval;
}

int
LocalRepo::addObjectRaw(const ObjectInfo &info, bytestream *bs)
{
    assert(opened);
    assert(info.hasAllFields());
    ObjectHash hash = info.hash;
    assert(!hash.isEmpty());
    string objPath = objIdToPath(hash);

    if (!Util_FileExists(objPath)) {
        createObjDirs(hash);

        LocalObject o;
        if (o.createFromRawData(objPath, info, bs->readAll()) < 0) {
            perror("Unable to create object");
            return -errno;
        }
    }

    index.updateInfo(hash, info);

    return 0;
}

/*
 * Add a tree to the repository.
 */
ObjectHash
LocalRepo::addTree(const Tree &tree)
{
    string blob = tree.getBlob();
    ObjectHash hash = Util_HashString(blob);

    if (hasObject(hash)) {
        return hash;
    }

    /*for (it = tree.tree.begin(); it != tree.tree.end(); it++) {
        LocalObject::sp o(getLocalObject((*it).second.hash));
        if (!o) {
            perror("Cannot open object");
            assert(false);
        }
        addBackref((*it).second.hash);
        //o.close();
    }*/

    return addBlob(Object::Tree, blob);
}

/*
 * Add a commit to the repository.
 */
ObjectHash
LocalRepo::addCommit(/* const */ Commit &commit)
{
    string blob = commit.getBlob();
    ObjectHash hash = Util_HashString(blob);
    string refPath;

    if (hasObject(hash)) {
        return hash;
    }

    /*addBackref(commit.getTree());

    refPath = commit.getParents().first;
    if (refPath != EMPTY_COMMIT) {
        addBackref(refPath);
    }

    refPath = commit.getParents().second;
    if (refPath != "") {
        addBackref(refPath);
        //o.close();
    }*/

    if (commit.getSnapshot() != "") {
	snapshots.addSnapshot(commit.getSnapshot(), hash);
    }

    return addBlob(Object::Commit, blob);
}

/*
 * Read an object into memory and return it as a string.
 */
std::string
LocalRepo::getPayload(const ObjectHash &objId)
{
    Object::sp o(getObject(objId));
    // XXX: Add better error handling
    if (!o)
        return "";

    // TODO: if object is a LargeBlob, this will only return the LargeBlob
    // object, not the full contents of all the referenced blobs

    auto_ptr<bytestream> bs(o->getPayloadStream());
    return bs->readAll();
}

/*
 * Get an object length.
 */
size_t
LocalRepo::getObjectLength(const ObjectHash &objId)
{
    if (!hasObject(objId)) {
        LOG("Couldn't get object %s", objId.hex().c_str());
        return -1;
    }

    const ObjectInfo &info = getObjectInfo(objId);
    return info.payload_size;
}

/*
 * Get the object type.
 */
Object::Type
LocalRepo::getObjectType(const ObjectHash &objId)
{
    if (!hasObject(objId)) {
        LOG("Couldn't get object %s", objId.hex().c_str());
        return Object::Null;
    }

    const ObjectInfo &info = getObjectInfo(objId);
    return info.type;
}

/*
 * Verify object
 */
string
LocalRepo::verifyObject(const ObjectHash &objId)
{
    LocalObject::sp o;
    Object::Type type;

    if (!hasObject(objId))
	return "Object not found!";

    // XXX: Add better error handling
    o = getLocalObject(objId);
    if (!o)
	return "Cannot open object!";

    type = o->getInfo().type;
    switch(type) {
	case Object::Null:
	    return "Object with Null type!";
	case Object::Commit:
	{
	    if (o->computeHash() != objId)
		return "Object hash mismatch!"; 

	    // XXX: Verify tree and parents exist
	    break;
	}
	case Object::Tree:
	{
	    if (o->computeHash() != objId)
		return "Object hash mismatch!"; 

            Tree t;
            t.fromBlob(o->getPayload());
            for (map<string, TreeEntry>::iterator it = t.tree.begin();
                    it != t.tree.end();
                    it++) {
                if (!(*it).second.hasBasicAttrs())
                    return string("TreeEntry ") + (*it).first + " missing basic attrs";
            }

	    // XXX: Verify subtrees and blobs exist
	    break;
	}
	case Object::Blob:
	{
	    if (o->computeHash() != objId)
		return "Object hash mismatch!"; 

	    break;
	}
        case Object::LargeBlob:
        {
	    if (o->computeHash() != objId)
		return "Object hash mismatch!"; 

            // XXX: Verify fragments
            // XXX: Verify file hash matches largeObject's file hash
            break;
        }
	case Object::Purged:
	    break;
	default:
	    return "Object with unknown type!";
    }

    // Check object metadata
    /*if (!o->checkMetadata()) {
        return "Object metadata hash mismatch!";
    }*/
    // TODO

    if (!o->getInfo().hasAllFields()) {
        return "Object info missing some fileds!";
    }

    // XXX: Check against index

    return "";
}

/*
 * Purge object
 */
bool
LocalRepo::purgeObject(const ObjectHash &objId)
{
    LocalObject::sp o = getLocalObject(objId);

    if (!o)
	return false;

    if (o->purge() < 0)
	return false;

    return true;
}

/*
 * Copy an object to a working directory.
 */
bool
LocalRepo::copyObject(const ObjectHash &objId, const string &path)
{
    LocalObject::sp o = getLocalObject(objId);

    // XXX: Add better error handling
    if (!o)
	return false;

    bytestream::ap bs(o->getPayloadStream());
    if (o->getInfo().type == Object::Blob) {
        if (bs->copyToFile(path) < 0)
	    return false;
    } else if (o->getInfo().type == Object::LargeBlob) {
        LargeBlob lb = LargeBlob(this);
        lb.fromBlob(bs->readAll());
        lb.extractFile(path);
    }
    return true;
}

/*void
LocalRepo::addBackref(const std::string &refers_to)
{
    if (refers_to == EMPTY_COMMIT) return;
    metadata.addRef(refers_to);
}*/

int
Repo_GetObjectsCB(void *arg, const char *path)
{
    string hexHash;
    set<ObjectInfo> *objs = (set<ObjectInfo> *)arg;

    // XXX: Only add the hash
    if (Util_IsDirectory(path)) {
	return 0;
    }

    hexHash = path;
    hexHash = hexHash.substr(hexHash.rfind("/") + 1);
    ObjectHash hash = ObjectHash::fromHex(hexHash);

    LocalObject o;
    o.open(path);
    ObjectInfo info = o.getInfo();
    info.hash = hash;
    objs->insert(info);

    return 0;
}

/*
 * Return a list of objects in the repository.
 */
set<ObjectInfo>
LocalRepo::slowListObjects()
{
    int status;
    set<ObjectInfo> objs;
    string objRoot = rootPath + ORI_PATH_OBJS;

    status = Scan_RTraverse(objRoot.c_str(), (void *)&objs, Repo_GetObjectsCB);
    if (status < 0)
	perror("listObjects");

    return objs;
}

set<ObjectInfo>
LocalRepo::listObjects()
{
    return index.getList();
}

bool
LocalRepo::rebuildIndex()
{
    set<ObjectInfo> l = slowListObjects();
    set<ObjectInfo>::iterator it;

    string indexPath = rootPath + ORI_PATH_INDEX;
    index.close();

    Util_DeleteFile(indexPath);

    index.open(indexPath);

    for (it = l.begin(); it != l.end(); it++)
    {
        index.updateInfo((*it).hash, (*it));
    }

    return true;
}

bool _timeCompare(const Commit &c1, const Commit &c2) {
    return c1.getTime() < c2.getTime();
}

vector<Commit>
LocalRepo::listCommits()
{
    vector<Commit> rval;

    // TODO: more efficient
    set<ObjectInfo> objs = listObjects();
    for (set<ObjectInfo>::iterator it = objs.begin();
            it != objs.end();
            it++) {
        const ObjectInfo &oi = *it;
        if (oi.type == Object::Commit) {
            const Commit &c = getCommit(oi.hash);
            rval.push_back(c);
        }
    }

    sort(rval.begin(), rval.end(), _timeCompare);
    return rval;
}

map<string, ObjectHash>
LocalRepo::listSnapshots()
{
    return snapshots.getList();
}

ObjectHash
LocalRepo::lookupSnapshot(const string &name)
{
    map<string, ObjectHash> snaps = snapshots.getList();
    map<string, ObjectHash>::iterator it = snaps.find(name);

    if (it != snaps.end())
	return (*it).second;

    return ObjectHash();
}

/*
 * High Level Operations
 */

/*
 * Pull changes from the source repository.
 */
void
LocalRepo::pull(Repo *r)
{
    SshRepo *sshrepo = NULL;
    if (dynamic_cast<SshRepo*>(r))
        sshrepo = (SshRepo *)r;

    vector<Commit> remoteCommits = r->listCommits();
    deque<ObjectHash> toPull;

    for (size_t i = 0; i < remoteCommits.size(); i++) {
        ObjectHash hash = remoteCommits[i].hash();
        if (!hasObject(hash)) {
            toPull.push_back(hash);
            // TODO: partial pull
        }
    }

    if (sshrepo)
        sshrepo->preload(toPull.begin(), toPull.end());

    LocalRepoLock::ap _lock(lock());

    // Perform the pull
    while (!toPull.empty()) {
        ObjectHash hash = toPull.front();
        toPull.pop_front();

        Object::sp o(r->getObject(hash));
        if (!o) {
            printf("Error getting object %s\n", hash.hex().c_str());
            continue;
        }

        // Enqueue the object's references
        Object::Type t = o->getInfo().type;
        /*printf("Pulling object %s (%s)\n", hash.c_str(),
                Object::getStrForType(t));*/

        vector<ObjectHash> newObjs;
        if (t == Object::Commit) {
            Commit c;
            c.fromBlob(o->getPayload());
            if (!hasObject(c.getTree())) {
                toPull.push_back(c.getTree());
                newObjs.push_back(c.getTree());
            }
        }
        else if (t == Object::Tree) {
            Tree t;
            t.fromBlob(o->getPayload());
            for (map<string, TreeEntry>::iterator it = t.tree.begin();
                    it != t.tree.end();
                    it++) {
                const ObjectHash &entry_hash = (*it).second.hash;
                if (!hasObject(entry_hash)) {
                    toPull.push_back(entry_hash);
                    newObjs.push_back(entry_hash);
                }
            }
        }
        else if (t == Object::LargeBlob) {
            LargeBlob lb(this);
            lb.fromBlob(o->getPayload());

            for (map<uint64_t, LBlobEntry>::iterator pit = lb.parts.begin();
                    pit != lb.parts.end();
                    pit++) {
                const ObjectHash &h = (*pit).second.hash;
                if (!hasObject(h)) {
                    toPull.push_back(h);
                    newObjs.push_back(h);
                }
            }
        }

        // Preload new objects
        if (sshrepo)
            sshrepo->preload(newObjs.begin(), newObjs.end());

        // Add the object to this repo
        copyFrom(o.get());
    }
}

/*
 * Commit from TreeDiff
 */

void
LocalRepo::addLargeBlobBackrefs(const LargeBlob &lb, MdTransaction::sp tr)
{
    for (map<uint64_t, LBlobEntry>::const_iterator it = lb.parts.begin();
            it != lb.parts.end();
            it++) {
        const LBlobEntry &lbe = (*it).second;

        metadata.addRef(lbe.hash, tr);
    }
}

void
LocalRepo::addTreeBackrefs(const Tree &t, MdTransaction::sp tr)
{
    for (map<string, TreeEntry>::const_iterator it = t.tree.begin();
            it != t.tree.end();
            it++) {
        const TreeEntry &te = (*it).second;

        metadata.addRef(te.hash, tr);

        if (metadata.getRefCount(te.hash) == 0) {
            // Only recurse if the subtree is newly-added
            if (te.type == TreeEntry::Tree) {
                Tree subtree = getTree(te.hash);
                addTreeBackrefs(subtree, tr);
            }
            else if (te.type == TreeEntry::LargeBlob) {
                LargeBlob lb(this);
                lb.fromBlob(getPayload(te.hash));
                addLargeBlobBackrefs(lb, tr);
            }
        }
    }
}

void
LocalRepo::addCommitBackrefs(const Commit &c, MdTransaction::sp tr)
{
    if (c.getParents().first != EMPTY_COMMIT)
        metadata.addRef(c.getParents().first, tr);
    if (!c.getParents().second.isEmpty())
        metadata.addRef(c.getParents().second, tr);
    
    metadata.addRef(c.getTree(), tr);
    if (metadata.getRefCount(c.getTree()) == 0) {
        Tree t = getTree(c.getTree());
        addTreeBackrefs(t, tr);
    }
}

void
LocalRepo::copyObjectsFromLargeBlob(Repo *other, const LargeBlob &lb)
{
    for (map<uint64_t, LBlobEntry>::const_iterator it = lb.parts.begin();
            it != lb.parts.end();
            it++) {
        const LBlobEntry &lbe = (*it).second;
        if (hasObject(lbe.hash)) {
            continue;
        }

        Object::sp o(other->getObject(lbe.hash));
        assert(o->getInfo().type == Object::Blob);
        assert(o->getInfo().payload_size == lbe.length);
        copyFrom(o.get());
    }
}

void
LocalRepo::copyObjectsFromTree(Repo *other, const Tree &t)
{
    for (map<string, TreeEntry>::const_iterator it = t.tree.begin();
            it != t.tree.end();
            it++) {
        const TreeEntry &te = (*it).second;
        if (hasObject(te.hash)) {
            continue;
        }

        Object::sp o(other->getObject(te.hash));
        if (!o) {
            LOG("Couldn't get object %s\n", te.hash.hex().c_str());
            continue;
        }

        copyFrom(o.get());

        if (te.type == TreeEntry::Tree) {
            Tree subtree;
            subtree.fromBlob(o->getPayload());
            copyObjectsFromTree(other, subtree);
        }
        else if (te.type == TreeEntry::LargeBlob) {
            LargeBlob lb(this);
            lb.fromBlob(o->getPayload());
            copyObjectsFromLargeBlob(other, lb);
        }
    }
}

ObjectHash
LocalRepo::commitFromTree(const ObjectHash &treeHash, Commit &c)
{
    assert(opened);

    Object::sp treeObj(getObject(treeHash));
    assert(treeObj->getInfo().type == Object::Tree);

    //LocalRepoLock::ap _lock(lock());

    // Make the commit object
    if (c.getMessage().size() == 0)
        c.setMessage("No message.");
    if (c.getTime() == 0)
        c.setTime(time(NULL));
    if (c.getUser().size() == 0) {
        string user = Util_GetFullname();
        c.setUser(user);
    }
    
    c.setTree(treeHash);
    c.setParents(getHead());

    ObjectHash commitHash = addCommit(c);

    // Update .ori/HEAD
    updateHead(commitHash);

    // Backrefs
    MdTransaction::sp tr(metadata.begin());
    addCommitBackrefs(c, tr);

    printf("Commit Hash: %s\nTree Hash: %s\n",
	   commitHash.hex().c_str(),
	   treeHash.hex().c_str());
    return commitHash;
}

ObjectHash
LocalRepo::commitFromObjects(const ObjectHash &treeHash, Repo *objects,
        Commit &c)
{
    assert(opened);

    Object::sp newTreeObj(objects->getObject(treeHash));
    assert(newTreeObj->getInfo().type == Object::Tree);

    LocalRepoLock::ap _lock(lock());
    copyFrom(newTreeObj.get());

    Tree newTree = getTree(treeHash);
    copyObjectsFromTree(objects, newTree);

    return commitFromTree(treeHash, c);
}

/*
 * Garbage Collect. Attempt to reduce wasted space from deleted objects and 
 * metadata.
 */
void
LocalRepo::gc()
{
    // Compact the index
    index.rewrite();

    // Compact the metadata log
    metadata.rewrite();
}

/*
 * Return true if the repository has the object.
 */
bool
LocalRepo::hasObject(const ObjectHash &objId)
{
    bool val = index.hasObject(objId);

    if (val && remoteRepo != NULL) {
	val = remoteRepo->hasObject(objId);
    }

    return val;
}

/*
 * Return ObjectInfo through the fast path.
 */
ObjectInfo
LocalRepo::getObjectInfo(const ObjectHash &objId)
{
    if (index.hasObject(objId)) {
	return index.getInfo(objId);
    }
    if (remoteRepo != NULL) {
	return remoteRepo->getObjectInfo(objId);
    }
    return ObjectInfo();
}

/*
 * BasicRepo implementation
 */

/*
 * Reference Counting Operations
 */

const MetadataLog &
LocalRepo::getMetadata() const
{
    return metadata;
}

/*
 * Construct a raw set of references. This is the slow path and should only
 * be used as part of recovery.
 */
RefcountMap
LocalRepo::recomputeRefCounts()
{
    set<ObjectInfo> obj = listObjects();
    RefcountMap rval;

    for (set<ObjectInfo>::iterator it = obj.begin();
            it != obj.end();
            it++) {
        const ObjectHash &hash = (*it).hash;
        switch ((*it).type) {
            case Object::Commit:
            {
                Commit c = getCommit(hash);
                
                rval[c.getTree()] += 1;
                if (c.getParents().first != EMPTY_COMMIT) {
                    rval[c.getParents().first] += 1;
                }
                if (!c.getParents().second.isEmpty()) {
                    rval[c.getParents().second] += 1;
                }
                
                break;
            }
            case Object::Tree:
            {
                Tree t = getTree(hash);
                map<string, TreeEntry>::iterator tt;

                for  (tt = t.tree.begin(); tt != t.tree.end(); tt++) {
                    ObjectHash h = (*tt).second.hash;
                    rval[h] += 1;
                }
                break;
            }
            case Object::LargeBlob:
            {
                LargeBlob lb(this);
                Object::sp o(getObject(hash));
                lb.fromBlob(o->getPayload());

                for (map<uint64_t, LBlobEntry>::iterator pit = lb.parts.begin();
                        pit != lb.parts.end();
                        pit++) {
                    ObjectHash h = (*pit).second.hash;
                    rval[h] += 1;
                }
                break;
            }
            case Object::Blob:
	    case Object::Purged:
                break;
            default:
                printf("Unsupported object type!\n");
                assert(false);
                break;
        }
    }

    return rval;
}

bool
LocalRepo::rewriteRefCounts(const RefcountMap &refs)
{
    metadata.rewrite(&refs);
    return true;
}

/*
 * Grafting Operations
 */

/*
 * Return a set of all objects references by a tree.
 */
set<ObjectHash>
LocalRepo::getSubtreeObjects(const ObjectHash &treeId)
{
    queue<ObjectHash> treeQ;
    set<ObjectHash> rval;

    treeQ.push(treeId);

    while (!treeQ.empty()) {
	map<string, TreeEntry>::iterator it;
	Tree t = getTree(treeQ.front());
	treeQ.pop();

	for (it = t.tree.begin(); it != t.tree.end(); it++) {
	    TreeEntry e = (*it).second;
	    set<ObjectHash>::iterator p = rval.find(e.hash);

	    if (p == rval.end()) {
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
set<ObjectHash>
LocalRepo::walkHistory(HistoryCB &cb)
{
    set<ObjectHash> rval;
    set<ObjectHash> curLevel;
    set<ObjectHash> nextLevel;
    set<ObjectHash>::iterator it;

    if (getHead() == EMPTY_COMMIT)
        return rval;

    nextLevel.insert(getHead());

    while (!nextLevel.empty()) {
	curLevel = nextLevel;
	nextLevel.clear();

	for (it = curLevel.begin(); it != curLevel.end(); it++) {
	    Commit c = getCommit(*it);
	    pair<ObjectHash, ObjectHash> p = c.getParents();
            ObjectHash val;

	    val = cb.cb(*it, &c);
            if (!val.isEmpty())
                rval.insert(val);

            if (p.first != EMPTY_COMMIT) {
	        nextLevel.insert(p.first);
	        if (!p.second.isEmpty())
		    nextLevel.insert(p.second);
            }
	}
    }

    return rval;
}

/*
 * Lookup a path given a Commit and return the object ID.
 */
TreeEntry
LocalRepo::lookupTreeEntry(const Commit &c, const string &path)
{
    vector<string> pv = Util_PathToVector(path);
    vector<string>::iterator it;
    TreeEntry entry = TreeEntry();

    // Set the hash to point to the root
    entry.hash = c.getTree();

    for (it = pv.begin(); it != pv.end(); it++) {
	map<string, TreeEntry>::iterator e;
        Tree t = getTree(entry.hash);
	e = t.tree.find(*it);
	if (e == t.tree.end()) {
	    entry = TreeEntry();
	    entry.type = TreeEntry::Null;
	    return entry;
	}
        entry = t.tree[*it];
    }

    return entry;
}

/*
 * Lookup a path given a Commit and return the object ID.
 */
ObjectHash
LocalRepo::lookup(const Commit &c, const string &path)
{
    vector<string> pv = Util_PathToVector(path);
    ObjectHash objId = c.getTree();

    if (pv.size() == 0)
	return ObjectHash();

    for (size_t i = 0; i < pv.size(); i++) {
	map<string, TreeEntry>::iterator e;
        Tree t = getTree(objId);
	e = t.tree.find(pv[i]);
	if (e == t.tree.end()) {
	    return ObjectHash();
	}
        objId = t.tree[pv[i]].hash;
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
    virtual ObjectHash cb(const ObjectHash &commitId, Commit *c)
    {
        ObjectHash treeId = src->lookup(*c, srcPath);
        set<ObjectHash> objects;
        set<ObjectHash>::iterator it;

        if (treeId.isEmpty())
            return ObjectHash();

        printf("Commit: %s\n", commitId.hex().c_str());
        printf("Tree: %s\n", treeId.hex().c_str());

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

        return ObjectHash();
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
    set<ObjectHash> changes;

    printf("Hello!\n");
    changes = r->walkHistory(cb);

    // XXX: TODO
    NOT_IMPLEMENTED(false);
}

/*
 * Working Directory Operations
 */

int
listBranchesHelper(void *arg, const char *path)
{
    string name = path;
    set<string> *rval = (set<string> *)arg;

    name = name.substr(name.find_last_of("/") + 1);
    rval->insert(name);

    return 0;
}

set<string>
LocalRepo::listBranches()
{
    string path = rootPath + ORI_PATH_HEADS;
    set<string> rval;

    Scan_Traverse(path.c_str(), &rval, listBranchesHelper);

    return rval;
}

string
LocalRepo::getBranch()
{
    char *branch = Util_ReadFile(rootPath + ORI_PATH_BRANCH, NULL);

    if (branch == NULL)
	return "";

    return branch;
}

/*
 * Create or select the current branch.
 *
 * XXX: Validate branch name
 */
void
LocalRepo::setBranch(const std::string &name)
{
    // Verify branch name
    set<string> branches = listBranches();
    set<string>::iterator e = branches.find(name);

    if (e == branches.end()) {
	string branchFile = rootPath + ORI_PATH_HEADS + name;
	ObjectHash head = getHead();
	printf("Creating branch '%s'\n", name.c_str());

	Util_WriteFile(head.hex().c_str(), ObjectHash::SIZE * 2, branchFile);
    }

    Util_WriteFile(name.c_str(), name.size(), rootPath + ORI_PATH_BRANCH);
}

/*
 * Get the working repository version.
 */
ObjectHash
LocalRepo::getHead()
{
    string headPath = rootPath + ORI_PATH_HEADS + getBranch();
    char *commitId = Util_ReadFile(headPath, NULL);
    // XXX: Leak!

    if (commitId == NULL) {
	return EMPTY_COMMIT;
    }

    return ObjectHash::fromHex(commitId);
}

/*
 * Update the working directory revision.
 */
void
LocalRepo::updateHead(const ObjectHash &commitId)
{
    string headPath = rootPath + ORI_PATH_HEADS + getBranch();

    assert(!commitId.isEmpty());

    Util_WriteFile(commitId.hex().c_str(), ObjectHash::SIZE * 2, headPath);
}

/*
 * Get the tree associated with the current HEAD
 */
Tree
LocalRepo::getHeadTree()
{
    Tree t;
    ObjectHash head = getHead();
    if (head != EMPTY_COMMIT) {
        Commit headCommit = getCommit(head);
        t = getTree(headCommit.getTree());
    }

    return t;
}

/*
 * General Operations
 */

TempDir::sp
LocalRepo::newTempDir()
{
    string dir_templ = getRootPath() + ORI_PATH_TMP + "tmp.XXXXXX";
    char templ[PATH_MAX];
    strncpy(templ, dir_templ.c_str(), PATH_MAX);

    if (mkdtemp(templ) == NULL) {
        perror("mkdtemp");
        return TempDir::sp();
    }
    printf("Temporary directory at %s\n", templ);
    return TempDir::sp(new TempDir(templ));
}

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
 * Peer Management
 */
map<string, Peer>
LocalRepo::getPeers()
{
    return peers;
}

bool
LocalRepo::addPeer(const string &name, const string &path)
{
    map<string, Peer>::iterator it = peers.find(name);

    if (it == peers.end()) {
	Peer p = Peer(rootPath + ORI_PATH_REMOTES + name);
	p.setUrl(path);
	peers[name] = p;
    } else {
	(*it).second.setUrl(path);
    }

    return true;
}

bool
LocalRepo::removePeer(const string &name)
{
    map<string, Peer>::iterator it = peers.find(name);

    if (it != peers.end()) {
	peers.erase(it);
	Util_DeleteFile(rootPath + ORI_PATH_REMOTES + name);
    }

    return true;
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

