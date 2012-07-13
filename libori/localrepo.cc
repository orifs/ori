#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>

#include <string>
#include <queue>
#include <set>
using namespace std;

#include "localrepo.h"
#include "largeblob.h"
#include "util.h"
#include "scan.h"
#include "debug.h"

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
        if (unlink(lockFile.c_str()) < 0) {
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
{
    rootPath = (root == "") ? findRootPath() : root;
}

LocalRepo::~LocalRepo()
{
    close();
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

    return true;
}

void
LocalRepo::close()
{
    index.close();
}

LocalRepoLock *
LocalRepo::lock()
{
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
 * Object Operations
 */

Object *LocalRepo::getObject(const std::string &objId)
{
    string path = objIdToPath(objId);
    LocalObject *o = new LocalObject();
    o->open(path, objId);

    return o;
}

LocalObject LocalRepo::getLocalObject(const std::string &objId)
{
    string path = objIdToPath(objId);
    LocalObject o;
    o.open(path, objId);
    return o;
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
	LocalObject o;

	createObjDirs(hash);
	if (o.create(objPath, Object::Blob) < 0) {
	    perror("Unable to create object");
	    return "";
	}
        diskstream ds(path);
	if (o.setPayload(&ds) < 0) {
	    perror("Unable to copy file");
	    return "";
	}

        index.updateInfo(hash, o.getInfo());
    }

    return hash;
}

/*
 * Add a file to the repository. This is a low-level interface.
 */
pair<string, string>
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
            LocalObject o;
            string refPath = objIdToPath((*it).second.hash);

            if (o.open(refPath) != 0) {
                perror("Cannot open object");
                assert(false);
            }
            o.addBackref(hash, Object::BRRef);
            o.close();
        }
    }

    return make_pair(addBlob(Object::LargeBlob, blob), lb.hash);
}

/*
 * Add a file to the repository. This is an internal interface that pusheds the
 * work to addLargeFile or addSmallFile based on our size threshold.
 */
pair<string, string>
LocalRepo::addFile(const string &path)
{
    size_t sz = Util_FileSize(path);

    if (sz > LARGEFILE_MINIMUM)
        return addLargeFile(path);
    else
        return make_pair(addSmallFile(path), "");
}

int
LocalRepo::addObjectRaw(const ObjectInfo &info, bytestream *bs)
{
    string hash = info.hash;
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
        LocalObject o;
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
    LocalObject o;
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
            printf("%s\n", refPath.c_str());
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
 * Read an object into memory and return it as a string.
 */
std::string
LocalRepo::getPayload(const string &objId)
{
    Object::ap o(getObject(objId));
    // XXX: Add better error handling
    if (!o.get())
        return "";

    // TODO: if object is a LargeBlob, this will only return the LargeBlob
    // object, not the full contents of all the referenced blobs

    auto_ptr<bytestream> bs(o->getPayloadStream());
    return bs->readAll();
}

/*
 * Verify object
 */
string
LocalRepo::verifyObject(const string &objId)
{
    string objPath = objIdToPath(objId);
    LocalObject o;
    Object::Type type;

    if (!hasObject(objId))
	return "Object not found!";

    // XXX: Add better error handling
    if (o.open(objPath) < 0)
	return "Cannot open object!";

    type = o.getInfo().type;
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
	    if (o.computeHash() != objId)
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
    if (!o.checkMetadata()) {
        return "Object metadata hash mismatch!";
    }

    if (!o.getInfo().hasAllFields()) {
        return "Object info missing some fileds!";
    }

    // XXX: Check against index

    return "";
}

/*
 * Purge object
 */
bool
LocalRepo::purgeObject(const string &objId)
{
    string objPath = objIdToPath(objId);
    LocalObject o;

    if (o.open(objPath) < 0)
	return false;

    if (o.purge() < 0)
	return false;

    return true;
}

/*
 * Copy an object to a working directory.
 */
bool
LocalRepo::copyObject(const string &objId, const string &path)
{
    string objPath = objIdToPath(objId);
    LocalObject o;

    // XXX: Add better error handling
    if (o.open(objPath) < 0)
	return false;

    bytestream::ap bs(o.getPayloadStream());
    if (o.getInfo().type == Object::Blob) {
        if (bs->copyToFile(path) < 0)
	    return false;
    } else if (o.getInfo().type == Object::LargeBlob) {
        LargeBlob lb = LargeBlob(this);
        lb.fromBlob(bs->readAll());
        lb.extractFile(path);
    }
    return true;
}

int
Repo_GetObjectsCB(void *arg, const char *path)
{
    string hash;
    set<ObjectInfo> *objs = (set<ObjectInfo> *)arg;

    // XXX: Only add the hash
    if (Util_IsDirectory(path)) {
	return 0;
    }

    hash = path;
    hash = hash.substr(hash.rfind("/") + 1);

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

Commit
LocalRepo::getCommit(const std::string &commitId)
{
    Commit c = Commit();
    string blob = getPayload(commitId);
    if (blob.size() == 0) {
        printf("Error getting commit blob\n");
        assert(false);
        return c;
    }
    c.fromBlob(blob);

    return c;
}

/*
 * Return true if the repository has the object.
 */
bool
LocalRepo::hasObject(const string &objId)
{
    return index.hasObject(objId);
}


/*
 * BasicRepo implementation
 */

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
    LocalObject o;
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
    set<ObjectInfo> obj = listObjects();
    set<ObjectInfo>::iterator it;
    map<string, map<string, Object::BRState> > rval;

    for (it = obj.begin(); it != obj.end(); it++) {
        rval[(*it).hash] = getRefs((*it).hash);
    }

    return rval;
}

/*
 * Construct a raw set of references. This is the slow path and should only
 * be used as part of recovery.
 */
LocalRepo::ObjReferenceMap
LocalRepo::computeRefCounts()
{
    set<ObjectInfo> obj = listObjects();
    set<ObjectInfo>::iterator it;
    map<string, set<string> > rval;
    map<string, set<string> >::iterator key;

    for (it = obj.begin(); it != obj.end(); it++) {
        const std::string &hash = (*it).hash;
        key = rval.find(hash);
        if (key == rval.end())
            rval[hash] = set<string>();
        switch ((*it).type) {
            case Object::Commit:
            {
                Commit c = getCommit(hash);
                
                key = rval.find(c.getTree());
                if (key == rval.end())
                    rval[c.getTree()] = set<string>();
                rval[c.getTree()].insert(hash);
                
                key = rval.find(c.getParents().first);
                if (key == rval.end())
                    rval[c.getParents().first] = set<string>();
                rval[c.getParents().first].insert(hash);
                
                if (c.getParents().second != "") {
                    key = rval.find(c.getParents().second);
                    if (key == rval.end())
                        rval[c.getTree()] = set<string>();
                    rval[c.getParents().second].insert(hash);
                }
                
                break;
            }
            case Object::Tree:
            {
                Tree t = getTree(hash);
                map<string, TreeEntry>::iterator tt;

                for  (tt = t.tree.begin(); tt != t.tree.end(); tt++) {
                    string h = (*tt).second.hash;
                    key = rval.find(h);
                    if (key == rval.end())
                        rval[h] = set<string>();
                    rval[h].insert(hash);
                }
                break;
            }
            case Object::LargeBlob:
            {
                LargeBlob lb(this);
                Object::ap o(getObject(hash));
                lb.fromBlob(o->getPayload());

                for (map<uint64_t, LBlobEntry>::iterator pit = lb.parts.begin();
                        pit != lb.parts.end();
                        pit++) {
                    string h = (*pit).second.hash;
                    key = rval.find(h);
                    if (key == rval.end())
                        rval[h] = set<string>();
                    rval[h].insert(hash);
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
LocalRepo::rewriteReferences(const LocalRepo::ObjReferenceMap &refs)
{
    LOG("Rebuilding references");
    LocalRepoLock::ap _lock(lock());

    for (ObjReferenceMap::const_iterator it = refs.begin();
            it != refs.end(); it++) {
        int status;
        LocalObject o;
        Object::Type type;

        if ((*it).first == EMPTY_COMMIT)
            continue;

        status = o.open(objIdToPath((*it).first));
        if (status < 0) {
            LOG("Cannot open object %s", (*it).first.c_str());
            return false;
        }
        type = o.getInfo().type;

        if (type == Object::Commit ||
            type == Object::Tree ||
            type == Object::Blob ||
            type == Object::LargeBlob) {
            set<string>::iterator i;

            o.clearMetadata(); // was clearBackref
            for (i = (*it).second.begin(); i != (*it).second.end(); i++) {
                o.addBackref((*i), Object::BRRef);
            }
        } else if (type == Object::Purged) {
            set<string>::iterator i;

            o.clearMetadata(); // was clearBackref
            for (i = (*it).second.begin(); i != (*it).second.end(); i++) {
                o.addBackref((*i), Object::BRPurged);
            }
        } else {

            NOT_IMPLEMENTED(false);
        }

        o.close();
    }

    return true;
}

bool
LocalRepo::stripMetadata()
{
    LOG("Stripping metadata");
    int status;
    LocalRepoLock::ap _lock(lock());
    set<ObjectInfo> obj = listObjects();
    set<ObjectInfo>::iterator it;

    for (it = obj.begin(); it != obj.end(); it++) {
        LocalObject o;

        status = o.open(objIdToPath((*it).hash));
        if (status < 0) {
            LOG("Cannot open object %s", (*it).hash.c_str());
            return false;
        }

        o.clearMetadata();
        o.close();
    }

    return true;
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

        printf("Commit: %s\n", commitId.c_str());
        printf("Tree: %s\n", treeId.c_str());

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

    printf("Hello!\n");
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

    assert(commitId.length() == 64);

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

