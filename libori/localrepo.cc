/*
 * Copyright (c) 2012-2013 Stanford University
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

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/bind.hpp>

using namespace std;

#include <ori/version.h>
#include <oriutil/debug.h>
#include <oriutil/runtimeexception.h>
#include <oriutil/systemexception.h>
#include <oriutil/oriutil.h>
#include <oriutil/orifile.h>
#include <oriutil/oristr.h>
#include <oriutil/oricrypt.h>
#include <oriutil/scan.h>
#include <oriutil/zeroconf.h>
#include <ori/largeblob.h>
#include <ori/localrepo.h>
#include <ori/sshrepo.h>
#include <ori/remoterepo.h>

#define ORI_DIR_MASK        0755
#define ORI_FILE_MASK       0644
#define ORI_ID_FILE_MASK    0444

int
LocalRepo_Init(const std::string &rootPath, bool bareRepo)
{
    string oriPath;
    string tmpDir;
    string objDir;
    string versionFile;
    string uuidFile;
    int fd;

    // Create directory
    if (bareRepo) {
        oriPath = rootPath;
    } else {
        oriPath = rootPath + ORI_PATH_DIR;
        if (mkdir(oriPath.c_str(), ORI_DIR_MASK) < 0) {
            perror("Could not create '.ori' directory");
            return 1;
        }
    }

    // Create tmp directory
    tmpDir = oriPath + "/tmp";
    if (mkdir(tmpDir.c_str(), ORI_DIR_MASK) < 0) {
        perror("Could not create '.ori/tmp' directory");
        return 1;
    }

    // Create objs directory
    tmpDir = oriPath + "/objs";
    if (mkdir(tmpDir.c_str(), ORI_DIR_MASK) < 0) {
        perror("Could not create '.ori/objs' directory");
        return 1;
    }

    // Create trusted keys directory
    tmpDir = oriPath + "/trusted";
    if (mkdir(tmpDir.c_str(), ORI_DIR_MASK) < 0) {
        perror("Could not create '.ori/trusted' directory");
        return 1;
    }

    // Create refs directory
    tmpDir = oriPath + "/refs";
    if (mkdir(tmpDir.c_str(), ORI_DIR_MASK) < 0) {
        perror("Could not create '.ori/refs' directory");
        return 1;
    }

    // Create refs/heads directory
    tmpDir = oriPath + "/refs/heads";
    if (mkdir(tmpDir.c_str(), ORI_DIR_MASK) < 0) {
        perror("Could not create '.ori/refs/heads' directory");
        return 1;
    }

    // Create default branch
    tmpDir = oriPath + "/default";
    if (!OriFile_WriteFile(EMPTY_COMMIT.hex(), tmpDir))
    {
	perror("Could not create default branch");
	return 1;
    }

    // Set branch name
    if (!OriFile_WriteFile("@default", 8, oriPath + ORI_PATH_HEAD)) {
	perror("Could not create branch file");
	return 1;
    }

    // Create refs/remotes directory
    tmpDir = oriPath + "/refs/remotes";
    if (mkdir(tmpDir.c_str(), ORI_DIR_MASK) < 0) {
        perror("Could not create '.ori/refs/remotes' directory");
        return 1;
    }

    // Construct UUID
    uuidFile = oriPath + ORI_PATH_UUID;
    fd = open(uuidFile.c_str(), O_CREAT|O_WRONLY|O_APPEND, ORI_FILE_MASK);
    if (fd < 0) {
        perror("Could not create UUID file");
        return 1;
    }

    std::string generated_uuid = Util_NewUUID();
    write(fd, generated_uuid.data(), generated_uuid.length());
    close(fd);
    chmod(uuidFile.c_str(), ORI_ID_FILE_MASK);

    // Construct version file
    versionFile = oriPath + ORI_PATH_VERSION;
    fd = open(versionFile.c_str(), O_CREAT|O_WRONLY|O_APPEND, ORI_FILE_MASK);
    if (fd < 0) {
        perror("Could not create version file");
        return 1;
    }
    write(fd, ORI_FS_VERSION_STR, strlen(ORI_FS_VERSION_STR));
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
        if (OriFile_Delete(lockFile) < 0) {
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
LocalRepo_PeerHelper(LocalRepo *l, const string &path)
{
    Peer p = Peer(path);

    string name = path;
    name = name.substr(name.find_last_of("/") + 1);

    // Skip journal files
    if (!OriStr_EndsWith(name, ".journal") && !OriStr_EndsWith(name, ".tmp")) {
        l->peers[name] = p;
    }

    return 0;
}

void
LocalRepo::open(const string &root)
{
    if (root.compare("") != 0) {
        rootPath = root;
    }

    if (rootPath.compare("") == 0)
        throw RuntimeException(ORIEC_INVALIDARGS, "Root path not set");

    try {
        // Read UUID
        std::string uuid_path = rootPath + ORI_PATH_UUID;
        id = OriFile_ReadFile(uuid_path);

        // Read Version
        version = OriFile_ReadFile(rootPath + ORI_PATH_VERSION);

        if (version != ORI_FS_VERSION_STR) {
            WARNING("LocalRepo::open: Unsupported file system version!");
            throw RuntimeException(ORIEC_UNSUPPORTEDVERSION, "Unsuppported file system version!");
        }
    }
    catch (std::ios_base::failure &e)
    {
        WARNING("LocalRepo::open: %s", e.what());
        throw SystemException();
    }

    // XXX: Check and rebuild index on error
    index.open(rootPath + ORI_PATH_INDEX); // throws SystemException or RuntimeException

    // Open snapshot index
    try {
        snapshots.open(rootPath + ORI_PATH_SNAPSHOTS); // throws SystemException
    } catch (exception &e) {
        index.close();
        throw e;
    }

    // Open Metadata Log
    try {
        metadata.open(rootPath + ORI_PATH_METADATA); // throws SystemException
    } catch (exception &e) {
        index.close();
        snapshots.close();
        throw e;
    }
    packfiles.reset(new PackfileManager(getRootPath() + ORI_PATH_OBJS));

    // Scan for peers
    string peer_path = rootPath + ORI_PATH_REMOTES;
    DirIterate(peer_path.c_str(), this, LocalRepo_PeerHelper);

    map<string, Peer>::iterator it;
    for (it = peers.begin(); it != peers.end(); it++) {
	if ((*it).second.isInstaCloning()) {
            // XXX: Open remote connection
	    if (resumeRepo.connect((*it).second.getUrl())) {
		LOG("Autoconnected to '%s' to resume InstaClone.",
		    (*it).second.getUrl().c_str());
                cacheRemoteObjects = true;
		remoteRepo = resumeRepo.get();
		break;
	    }
	}
    }

    opened = true;
}

void
LocalRepo::close()
{
    if (!opened)
        return;

    sync();

    currTransaction.reset();
    index.close();
    snapshots.close();
    packfiles.reset();
    opened = false;
}

LocalRepoLock::sp
LocalRepo::lock()
{
    ASSERT(opened);
    if (rootPath == "")
        return LocalRepoLock::sp();

    if (repoProcessLock.get()) {
        return repoProcessLock;
    }

    std::string lfPath = rootPath + ORI_PATH_LOCK;
    char pnum_str[64];
    sprintf(pnum_str, "%u", getpid());

    int rval = symlink(pnum_str, lfPath.c_str());
    if (rval < 0) {
        if (errno == EEXIST) {
            ssize_t n = readlink(lfPath.c_str(), pnum_str, 63);
            pnum_str[n] = '\0';
            fprintf(stderr, "Repository at %s is already locked\n",
                    rootPath.c_str());
            fprintf(stderr, "Another instance of ORI (pid %s) may currently be using it\n", pnum_str);
            OriDebug_PrintBacktrace();
        } else {
            perror("symlink");
        }

        exit(1);
    }

    repoProcessLock.reset(new LocalRepoLock(lfPath));
    return repoProcessLock;
}

/*
 * Remote Operations
 */
void
LocalRepo::setRemote(Repo *r)
{
    ASSERT(remoteRepo == NULL);
    remoteRepo = r;
    cacheRemoteObjects = true;
}

void
LocalRepo::clearRemote()
{
    remoteRepo = NULL;
}

void
LocalRepo::setRemoteFlags(bool cacheLocally)
{
    cacheRemoteObjects = cacheLocally;
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
        LOG("Object not found: %s", objId.hex().c_str());
        if (remoteRepo != NULL) {
            LOG("Instaclone getting object %s", objId.hex().c_str());
            Object::sp ro = remoteRepo->getObject(objId);

            if (!ro) {
                LOG("Object not available on remote machine!");
                return Object::sp();
            }

            if (cacheRemoteObjects) {
                auto_ptr<bytestream> bs(ro->getPayloadStream());
                string buf = bs->readAll();
                addBlob(ro->getInfo().type, buf);

                // XXX: Add reference counts
            }

            return ro;
        } else {
            return Object::sp();
        }
    }

    return Object::sp(o);
}

// XXX: Verify and recover from corrupt objects!!!
// XXX: Why do we check compression in Packfile::getPayload
// XXX: LocalObject::getStream and transactions multiple places.
LocalObject::sp LocalRepo::getLocalObject(const ObjectHash &objId)
{
    ASSERT(opened);

    if (currTransaction.get()) {
        if (currTransaction->has(objId)) {
            return LocalObject::sp(new LocalObject(currTransaction,
                        currTransaction->hashToIx[objId]));
        }
    }

    /*
     * The object may not be present locally as is the case with
     * instacloning.
     */
    if (!index.hasObject(objId))
	return LocalObject::sp();

    const IndexEntry &ie = index.getEntry(objId);
    Packfile::sp packfile = packfiles->getPackfile(ie.packfile);
    return LocalObject::sp(new LocalObject(packfile, ie));
}

void
LocalRepo::createObjDirs(const ObjectHash &objId)
{
    string path = rootPath;
    string hexId = objId.hex();

    ASSERT(path != "");

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

    ASSERT(!objId.isEmpty());
    ASSERT(hexId.length() == 64);
    ASSERT(rval != "");

    rval += ORI_PATH_OBJS;
    rval += hexId.substr(0,2);
    rval += "/";
    rval += hexId.substr(2,2);
    rval += "/";
    rval += hexId;

    return rval;
}

int
LocalRepo::addObject(ObjectType type, const ObjectHash &hash,
        const std::string &payload)
{
    ASSERT(opened);
    ASSERT(!hash.isEmpty());

    purged.erase(hash);

    if (isObjectStored(hash)) return 0;

    if (!currPackfile.get()) {
        currPackfile = packfiles->newPackfile();
        currTransaction = currPackfile->begin(&index);
    }

    if (!currTransaction.get()) {
        currTransaction = currPackfile->begin(&index);
    }

    if (currTransaction->full()) {
        currTransaction->commit();
        currTransaction.reset();
        currPackfile = packfiles->newPackfile();
        currTransaction = currPackfile->begin(&index);
    }

    ObjectInfo info(hash);
    info.type = type;
    info.payload_size = payload.size();

    currTransaction->addPayload(info, payload);


    /*string objPath = objIdToPath(hash);

    if (!OriFile_Exists(objPath)) {
        createObjDirs(hash);

        LocalObject o;
        if (o.createFromRawData(objPath, info, bs->readAll()) < 0) {
            perror("Unable to create object");
            return -errno;
        }
    }

    index.updateInfo(hash, newInfo);*/

    return 0;
}

/*
 * Add a tree to the repository.
 */
ObjectHash
LocalRepo::addTree(const Tree &tree)
{
    string blob = tree.getBlob();
    ObjectHash hash = OriCrypt_HashString(blob);

    if (hasObject(hash)) {
        return hash;
    }

    /*for (it = tree.tree.begin(); it != tree.tree.end(); it++) {
        LocalObject::sp o(getLocalObject((*it).second.hash));
        if (!o) {
            perror("Cannot open object");
            ASSERT(false);
        }
        addBackref((*it).second.hash);
        //o.close();
    }*/

    return addBlob(ObjectInfo::Tree, blob);
}

/*
 * Add a commit to the repository.
 */
ObjectHash
LocalRepo::addCommit(/* const */ Commit &commit)
{
    string blob = commit.getBlob();
    ObjectHash hash = OriCrypt_HashString(blob);
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

    return addBlob(ObjectInfo::Commit, blob);
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
ObjectType
LocalRepo::getObjectType(const ObjectHash &objId)
{
    if (!hasObject(objId)) {
        LOG("Couldn't get object %s", objId.hex().c_str());
        return ObjectInfo::Null;
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
    ObjectType type;

    if (!hasObject(objId))
	return "Object not found!";

    // XXX: Add better error handling
    o = getLocalObject(objId);
    if (!o)
	return "Cannot open object!";

    type = o->getInfo().type;
    if (type == ObjectInfo::Null)
        return "Object with Null type!";

    ObjectHash computedHash = OriCrypt_HashString(o->getPayload());
    if (computedHash != objId) {
        stringstream ss;
        ss << "Object hash mismatch! (computed hash "
           << computedHash.hex()
           << ")";
        return ss.str();
    }

    switch(type) {
	case ObjectInfo::Commit:
	{
	    // XXX: Verify tree and parents exist
	    break;
	}
	case ObjectInfo::Tree:
	{
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
	case ObjectInfo::Blob:
	{
	    break;
	}
        case ObjectInfo::LargeBlob:
        {
            // XXX: Verify fragments
            // XXX: Verify file hash matches largeObject's file hash
            break;
        }
	case ObjectInfo::Purged:
	    break;
	default:
	    return "Object with unknown type!";
    }

    if (!o->getInfo().hasAllFields()) {
        return "Object info missing some fileds!";
    }

    // XXX: Check against index

    return "";
}

/*
 * Copy an object to a working directory.
 */
bool
LocalRepo::copyObject(const ObjectHash &objId, const string &path)
{
    Object::sp o = getObject(objId);

    // XXX: Add better error handling
    if (!o) {
        WARNING("LocalRepo::copyObject failed to access object.\n");
        return false;
    }

    bytestream::ap bs(o->getPayloadStream());
    if (o->getInfo().type == ObjectInfo::Blob) {
        if (bs->copyToFile(path) < 0)
            return false;
    } else if (o->getInfo().type == ObjectInfo::LargeBlob) {
        LargeBlob lb = LargeBlob(this);
        lb.fromBlob(bs->readAll());
        lb.extractFile(path);
    }
    return true;
}

set<ObjectInfo>
LocalRepo::listObjects()
{
    return index.getList();
}

/*
 * This gu
 */
void
LocalRepo::sync()
{
    bool full = true;
    if (currTransaction.get()) {
        full = currTransaction->full();
        currTransaction->commit();
        currTransaction.reset();
        index.sync();
        metadata.sync();
    }
    if (full) {
        currPackfile = packfiles->newPackfile();
        currTransaction = currPackfile->begin(&index);
    }
}

struct RebuildIndexStruct
{
    Index *idx;
    packid_t id;
};

void
rebuildIndexCb(const ObjectInfo &info, offset_t off, void *arg)
{
    RebuildIndexStruct *ris = (RebuildIndexStruct *)arg;
    struct IndexEntry entry;

    entry.info = info;
    entry.offset = off;
    entry.packed_size = info.payload_size;
    entry.packfile = ris->id;

    ris->idx->updateEntry(info.hash, entry);
}

bool
LocalRepo::rebuildIndex()
{
    string indexPath = rootPath + ORI_PATH_INDEX;
    index.close();

    OriFile_Delete(indexPath);

    index.open(indexPath);

    vector<packid_t> pfIds = packfiles->getPackfileList();
    vector<packid_t>::iterator it;

    for (it = pfIds.begin(); it != pfIds.end(); it++)
    {
        RebuildIndexStruct ris;
        Packfile::sp pf = packfiles->getPackfile(*it);

        ris.idx = &index;
        ris.id = *it;
        pf->readEntries(rebuildIndexCb, (void *)&ris);
    }
    
    return true;
}

void
LocalRepo::dumpIndex()
{
    index.dump();
}

void
packfileDumper(const ObjectInfo &info, offset_t off, void *arg)
{
    info.print();
    printf("  packfile: offset = 0x%x\n", off);
}

void
LocalRepo::dumpPackfile(packid_t id)
{
    if (!packfiles->hasPackfile(id)) {
        printf("Packfile %d does not exist!\n", id);
        return;
    }

    printf("Dumping Packfile %d\n", id);
    Packfile::sp packfile = packfiles->getPackfile(id);
    packfile->readEntries(packfileDumper, NULL);
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
        if (oi.type == ObjectInfo::Commit) {
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
    vector<Commit> remoteCommits = r->listCommits();
    deque<ObjectHash> toPull;

    for (size_t i = 0; i < remoteCommits.size(); i++) {
        ObjectHash hash = remoteCommits[i].hash();
        if (!hasObject(hash)) {
            toPull.push_back(hash);
            // TODO: partial pull
        }
    }

    LocalRepoLock::sp _lock(lock());

    bytestream::ap objs(r->getObjects(toPull));
    receive(objs.get());

    // Perform the pull
    while (!toPull.empty()) {
        ObjectHash hash = toPull.front();
        toPull.pop_front();

        Object::sp o(getObject(hash));
        if (!o) {
            printf("Error getting object %s\n", hash.hex().c_str());
            continue;
        }

        // Enqueue the object's references
        ObjectType t = o->getInfo().type;
        /*printf("Pulling object %s (%s)\n", hash.c_str(),
                Object::getStrForType(t));*/

        vector<ObjectHash> newObjs;
        if (t == ObjectInfo::Commit) {
            Commit c;
            c.fromBlob(o->getPayload());
            if (!hasObject(c.getTree())) {
                toPull.push_back(c.getTree());
                newObjs.push_back(c.getTree());
            }
        } else if (t == ObjectInfo::Tree) {
            Tree t;
            t.fromBlob(o->getPayload());
            for (map<string, TreeEntry>::iterator it = t.tree.begin();
                    it != t.tree.end();
                    it++) {
                const ObjectHash &entry_hash = (*it).second.hash;
                if (!hasObject(entry_hash)) {
                    if ((*it).second.type != TreeEntry::Blob) {
                        toPull.push_back(entry_hash);
                    }
                    newObjs.push_back(entry_hash);
                }
            }
        } else if (t == ObjectInfo::LargeBlob) {
            LargeBlob lb(this);
            lb.fromBlob(o->getPayload());

            for (map<uint64_t, LBlobEntry>::iterator pit = lb.parts.begin();
                    pit != lb.parts.end();
                    pit++) {
                const ObjectHash &h = (*pit).second.hash;
                if (!hasObject(h)) {
                    //toPull.push_back(h);
                    newObjs.push_back(h);
                }
            }
        }

        if (newObjs.size() > 0) {
            objs.reset(r->getObjects(newObjs));
            receive(objs.get());
        }
    }
}


struct MultiPullOp {
    MultiPullOp(LocalRepo &r)
        : repo(r)
    {
    }
    LocalRepo &repo;

    // Pull queue
    deque<ObjectHash> toPull;
    tr1::unordered_set<ObjectHash> toPullSet;

    // Remotes
    std::vector<RemoteRepo::sp> remotes;
    std::vector<int> distances;
    vector<ObjectHashVec> toMultiPull;

    std::set<std::string> hostnames;

    void addCandidate(const OriPeer &peer) {
        std::stringstream ss;
        ss << "http://" << peer.hostname << ":" << peer.port << "/";

        if (hostnames.find(ss.str()) != hostnames.end()) {
            return;
        }

        RemoteRepo::sp remote(new RemoteRepo());
        if (!remote->connect(ss.str())) {
            fprintf(stderr, "Error connecting to %s\n", ss.str().c_str());
            return;
        }
        int dist = remote->get()->distance();
        hostnames.insert(ss.str());

        // Insert in sorted order
        bool inserted = false;
        for (size_t i = 0; i < distances.size(); i++) {
            if (distances[i] >= dist) {
                distances.insert(distances.begin()+i, dist);
                remotes.insert(remotes.begin()+i, remote);
                toMultiPull.insert(toMultiPull.begin()+i, ObjectHashVec());

                inserted = true;
                break;
            }
        }
        if (!inserted) {
            distances.push_back(dist);
            remotes.push_back(remote);
            toMultiPull.push_back(ObjectHashVec());
        }

        fprintf(stderr, "Discovered new peer %s (dist %d), now %lu peers\n",
                ss.str().c_str(), dist, distances.size());
    }

    void enqueue(const ObjectHash &hash) {
        if (repo.hasObject(hash)) return;
        if (toPullSet.find(hash) != toPullSet.end()) return;
        toPull.push_back(hash);
        toPullSet.insert(hash);
    }
};

void
LocalRepo::multiPull(RemoteRepo::sp defaultRemote)
{
    MultiPullOp mpo(*this);

    struct event_base *evbase = event_base_new();
#ifndef WITHOUT_MDNS 
    struct event *mdns_event = MDNS_Browse(evbase);
    event_add(mdns_event, NULL);
    MDNS_RegisterBrowseCallback(boost::bind(&MultiPullOp::addCandidate,
                &mpo, _1));
#endif

    mpo.remotes.push_back(defaultRemote);
    mpo.distances.push_back(defaultRemote->get()->distance());
    mpo.toMultiPull.push_back(ObjectHashVec());

    event_base_loop(evbase, EVLOOP_NONBLOCK);


    // Commits to pull
    vector<Commit> remoteCommits = defaultRemote->get()->listCommits();
    for (size_t i = 0; i < remoteCommits.size(); i++) {
        ObjectHash hash = remoteCommits[i].hash();
        mpo.enqueue(hash);
        fprintf(stderr, "Adding %s (commit)\n", hash.hex().c_str());
        // TODO: partial pull
    }

    size_t totalObjs = 0, closerObjs = 0;
    LocalRepoLock::sp _lock(lock());

    while (!mpo.toPull.empty()) {
        // Consolidate pulls from remote repos
        while (!mpo.toPull.empty()) {
            // Look for new peers
            event_base_loop(evbase, EVLOOP_NONBLOCK);

            ObjectHash hash = mpo.toPull.front();
            mpo.toPull.pop_front();

            // Find a source for this object
            bool found = false;
            for (size_t i = 0; i < mpo.distances.size(); i++) {
                const RemoteRepo::sp &remote = mpo.remotes[i];
                if (remote->get()->hasObject(hash)) {
                    mpo.toMultiPull[i].push_back(hash);
                    found = true;
                    if (remote.get() != defaultRemote.get())
                        closerObjs++;
                    break;
                }
            }
            if (!found) {
                fprintf(stderr, "No source for %s\n",
                        hash.hex().c_str());
                // TODO: keep retrying?
                mpo.toPull.push_back(hash);
                sleep(1);
                continue;
            }

            totalObjs++;
        }

        assert(mpo.toPull.size() == 0);
        mpo.toPullSet.clear();

        // Perform the pulls
        for (size_t i = 0; i < mpo.toMultiPull.size(); i++) {
            ObjectHashVec &hashes = mpo.toMultiPull[i];
            if (hashes.size() == 0) continue;

            fprintf(stderr, "Pulling %lu objects from %s\n", hashes.size(),
                    mpo.remotes[i]->getURL().c_str());
            bytestream::ap bs(mpo.remotes[i]->get()->getObjects(hashes));
            receive(bs.get());

            // Load more objects
            for (size_t ix_h = 0; ix_h < hashes.size(); ix_h++) {
                LocalObject::sp obj(getLocalObject(hashes[ix_h]));
                ObjectType t = obj->getInfo().type;

                if (t == ObjectInfo::Commit) {
                    Commit c;
                    c.fromBlob(obj->getPayload());
                    mpo.enqueue(c.getTree());
                }
                else if (t == ObjectInfo::Tree) {
                    Tree t;
                    t.fromBlob(obj->getPayload());
                    for (map<string, TreeEntry>::iterator it = t.tree.begin();
                            it != t.tree.end();
                            it++) {
                        const ObjectHash &entry_hash = (*it).second.hash;
                        mpo.enqueue(entry_hash);
                    }
                }
                else if (t == ObjectInfo::LargeBlob) {
                    LargeBlob lb(this);
                    lb.fromBlob(obj->getPayload());

                    for (map<uint64_t, LBlobEntry>::iterator pit = lb.parts.begin();
                            pit != lb.parts.end();
                            pit++) {
                        const ObjectHash &h = (*pit).second.hash;
                        mpo.enqueue(h);
                    }
                }
            }

            hashes.clear();
        }
    }

    printf("Speed-up: %lu of %lu objects\n", closerObjs, totalObjs);
}

void
LocalRepo::transmit(bytewstream *bs, const ObjectHashVec &objs)
{
    std::tr1::unordered_set<ObjectHash> includedHashes;

    typedef std::vector<IndexEntry> IndexEntryVec;
    std::map<Packfile::sp, IndexEntryVec> packs;
    for (size_t i = 0; i < objs.size(); i++) {
        if (includedHashes.find(objs[i]) == includedHashes.end()) {
            const IndexEntry &ie = index.getEntry(objs[i]);
            Packfile::sp pf = packfiles->getPackfile(ie.packfile);
            packs[pf].push_back(ie);
            includedHashes.insert(objs[i]);
        } else {
            DLOG("duplicate object in LocalRepo::transmit");
        }
    }

    for (std::map<Packfile::sp, IndexEntryVec>::iterator it = packs.begin();
            it != packs.end();
            it++) {
        const Packfile::sp &pf = (*it).first;
        //fprintf(stderr, "Transmitting %lu objects from %p\n",
        //        (*it).second.size(), pf.get());
        pf->transmit(bs, (*it).second);
    }

    /* Write (numobjs_t)0 */
    bs->writeUInt32(0);
}

void
LocalRepo::receive(bytestream *bs)
{
    bool cont = true;
    while (cont) {
        if (!currPackfile.get() || currPackfile->full()) {
            currPackfile = packfiles->newPackfile();
        }
        cont = currPackfile->receive(bs, &index);
    }
}

bytestream *
LocalRepo::getObjects(const ObjectHashVec &objs)
{
    strwstream ss;
    transmit(&ss, objs);
    return new strstream(ss.str());
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
    
    ObjectHash treeHash = c.getTree();
    if (treeHash.isEmpty()) {
        throw std::runtime_error("Commit tree is empty!");
    }

    metadata.addRef(treeHash, tr);
    if (metadata.getRefCount(treeHash) == 0) {
        Tree t = getTree(treeHash);
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
        ASSERT(o->getInfo().type == ObjectInfo::Blob);
        ASSERT(o->getInfo().payload_size == lbe.length);
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
LocalRepo::commitFromTree(const ObjectHash &treeHash, Commit &c,
        const std::string &status)
{
    ASSERT(opened);

    Object::sp treeObj(getObject(treeHash));
    ASSERT(treeObj->getInfo().type == ObjectInfo::Tree);

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
    if (hasMergeState()) {
	MergeState m = getMergeState();
	c.setParents(m.getParents().first, m.getParents().second);
    } else {
	c.setParents(getHead());
    }

    try {
        // XXX: Support password for private keys
        PrivateKey pkey = getPrivateKey();

        c.sign(pkey);
    } catch (exception &e) {
        // No private key
    }

    ObjectHash commitHash = addCommit(c);

    // Backrefs
    MdTransaction::sp tr(metadata.begin());
    addCommitBackrefs(c, tr);
    tr->setMeta(commitHash, "status", status);

    // Update .ori/HEAD
    if (status == "normal") {
        updateHead(commitHash);

        // Remove merge state
        /*
         * XXX: During a crash it's possible the merge state needs to be 
         * removed, because the commit has been updated.
         */
        if (hasMergeState()) {
            clearMergeState();
        }
    }

    LOG("Commit Hash: %s", commitHash.hex().c_str());
    LOG("Tree Hash: %s", treeHash.hex().c_str());
    return commitHash;
}

ObjectHash
LocalRepo::commitFromObjects(const ObjectHash &treeHash, Repo *objects,
        Commit &c, const std::string &status)
{
    ASSERT(opened);

    Object::sp newTreeObj(objects->getObject(treeHash));
    ASSERT(newTreeObj->getInfo().type == ObjectInfo::Tree);

    LocalRepoLock::sp _lock(lock());
    copyFrom(newTreeObj.get());

    Tree newTree = getTree(treeHash);
    copyObjectsFromTree(objects, newTree);

    return commitFromTree(treeHash, c, status);
}

/*
 * Garbage Collect. Attempt to reduce wasted space from deleted objects and 
 * metadata.
 */
void
LocalRepo::gc()
{
    // Commit all ongoing transactions
    if (currTransaction.get()) {
        currTransaction->commit();
        currTransaction.reset();
    }

    // Compact the index
    index.rewrite();

    // Compact the metadata log
    metadata.rewrite();

    // Do purges
    std::set<packid_t> purgePacks;
    for (std::set<ObjectHash>::iterator it = purged.begin();
            it != purged.end();
            it++) {
        const IndexEntry &ie = index.getEntry((*it));
        purgePacks.insert(ie.packfile);
    }

    for (std::set<packid_t>::iterator it = purgePacks.begin();
            it != purgePacks.end();
            it++) {
        Packfile::sp pack = packfiles->getPackfile((*it));
        pack->purge(purged, &index);
    }

    purged.clear();
}

/*
 * Return true if the repository has the object.
 */
bool
LocalRepo::isObjectStored(const ObjectHash &objId)
{
    if (currTransaction.get() && currTransaction->has(objId)) {
        return true;
    }

    return index.hasObject(objId);
}

bool
LocalRepo::hasObject(const ObjectHash &objId)
{
    if (isObjectStored(objId))
        return true;

    if (remoteRepo != NULL) {
	return remoteRepo->hasObject(objId);
    }

    return false;
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
 * Reference Counting Operations
 */

MetadataLog &
LocalRepo::getMetadata()
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
            case ObjectInfo::Commit:
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
            case ObjectInfo::Tree:
            {
                Tree t = getTree(hash);
                map<string, TreeEntry>::iterator tt;

                for  (tt = t.tree.begin(); tt != t.tree.end(); tt++) {
                    ObjectHash h = (*tt).second.hash;
                    rval[h] += 1;
                }
                break;
            }
            case ObjectInfo::LargeBlob:
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
            case ObjectInfo::Blob:
	    case ObjectInfo::Purged:
                break;
            default:
                printf("Unsupported object type!\n");
                PANIC();
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
 * Purging Operations
 */

/*
 * Purge object
 */
bool
LocalRepo::purgeObject(const ObjectHash &objId)
{
    ASSERT(metadata.getRefCount(objId) == 0);

    if (currTransaction.get())
        currTransaction.reset();

    /*const IndexEntry &ie = index.getEntry(objId);
    Packfile::sp packfile = packfiles->getPackfile(ie.packfile);
    packfile->purge(objId);*/

    purged.insert(objId);

    return true;
}

/*
 * Purge commit
 */
void
LocalRepo::decrefLB(const ObjectHash &lbhash, MdTransaction::sp tr)
{
    tr->decRef(lbhash);
    if (metadata.getRefCount(lbhash) == 1) {
        // Going to be purged, decref children
        LargeBlob lb(this);
        lb.fromBlob(getPayload(lbhash));
        for (std::map<uint64_t, LBlobEntry>::iterator it = lb.parts.begin();
                it != lb.parts.end();
                it++) {
            const LBlobEntry &entry = (*it).second;
            tr->decRef(entry.hash);
        }
    }
}

void
LocalRepo::decrefTree(const ObjectHash &thash, MdTransaction::sp tr)
{
    tr->decRef(thash);
    if (metadata.getRefCount(thash) == 1) {
        // Going to be purged, decref children
        Tree t = getTree(thash);
        for (std::map<std::string, TreeEntry>::iterator it = t.tree.begin();
                it != t.tree.end();
                it++) {
            const TreeEntry &te = (*it).second;
            if (te.type == TreeEntry::Tree) {
                decrefTree(te.hash, tr);
            }
            else if (te.type == TreeEntry::LargeBlob) {
                decrefLB(te.hash, tr);
            }
            else {
                tr->decRef(te.hash);
            }
        }
    }
}

bool
LocalRepo::purgeCommit(const ObjectHash &commitId)
{
    const Commit c = getCommit(commitId);
    ObjectHash rootTree;

    // Check all branches
    if (commitId == getHead()) {
	LOG("Cannot purge head of a branch");
	return false;
    }

    rootTree = c.getTree();

    MdTransaction::sp tx = metadata.begin();
    // Drop reference counts
    decrefTree(rootTree, tx);
    tx->setMeta(commitId, "status", "purging");
    tx.reset();

    // Purge objects -- TODO Needs to be journaled this is racey
    std::set<ObjectHash> objs = getSubtreeObjects(rootTree);
    for (std::set<ObjectHash>::iterator it = objs.begin(); it != objs.end(); it++) {
	if (metadata.getRefCount(*it) == 0)
	    purgeObject(*it);
    }

    tx = metadata.begin();
    tx->setMeta(commitId, "status", "purged");
    tx.reset();

    return true;
}

/*
 * Purge fuse commits
 */
void
LocalRepo::purgeFuseCommits()
{
    vector<Commit> commits = listCommits();
    for (size_t i = 0; i < commits.size(); i++) {
        const Commit &c = commits[i];
        ObjectHash hash = c.hash();
        if (metadata.getMeta(hash, "status") == "fuse") {
            bool status UNUSED = purgeCommit(hash);
            ASSERT(status);
        }
    }
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
                } else if (e.type == TreeEntry::LargeBlob) {
                    LargeBlob lb = getLargeBlob(e.hash);
                    std::map<uint64_t, LBlobEntry>::iterator it;
                    for (it = lb.parts.begin(); it != lb.parts.end(); it++) {
                        rval.insert(it->second.hash);
                    }
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
    entry.type = TreeEntry::Tree;
    entry.hash = c.getTree();

    for (it = pv.begin(); it != pv.end(); it++) {
	map<string, TreeEntry>::iterator e;
        Tree t = getTree(entry.hash);
	e = t.tree.find(*it);
	if (e == t.tree.end()) {
	    entry = TreeEntry();
	    entry.type = TreeEntry::Null;
	    entry.hash = ObjectHash(); // Set empty hash
	    return entry;
	}
        entry = (*e).second;
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

    if (path == "/")
        return objId;

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

class GraftDAGObject
{
public:
    GraftDAGObject()
	: hash(), commit(), objEntry(), objs(),
	  pFirst(), pSecond(), commitHash()
    {
    }
    GraftDAGObject(ObjectHash h, Commit c)
	: hash(h), commit(c), objEntry(), objs(),
	  pFirst(), pSecond(), commitHash()
    {
    }
    ~GraftDAGObject()
    {
    }
    bool setPath(const string &path, LocalRepo *srcRepo)
    {
	objEntry = srcRepo->lookupTreeEntry(commit, path);

	if (objEntry.hash.isEmpty())
	{
	    // Commit does not contain this path!
	    // XXX: Set type based on tip or don't commit
	    objEntry.hash = EMPTYFILE_HASH;
	    objs.insert(EMPTYFILE_HASH);
	    return true;
	}

	if (objEntry.isTree()) {
	    objs = srcRepo->getSubtreeObjects(objEntry.hash);
	} else if (objEntry.type == TreeEntry::LargeBlob) {
	    NOT_IMPLEMENTED(false);
	    // XXX: Add fragments to objs list
	} else {
	    objs.insert(objEntry.hash);
	}

	return true;
    }
    bool isEmpty() const
    {
	return objEntry.hash.isEmpty();
    }
    const ObjectHash getHash() const
    {
	return commitHash;
    }
    void setParents(const ObjectHash &p1 = EMPTY_COMMIT,
		    const ObjectHash &p2 = EMPTY_COMMIT)
    {
	pFirst = p1;
	pSecond = p2;
    }
    Commit graft(LocalRepo *srcRepo, LocalRepo *dstRepo,
	             const string &srcPath, const string &dstPath)
    {
	Commit tip;
	Commit c = Commit();
	ObjectHash treeHash;
	Tree::Flat fdtree;

	if (isEmpty()) {
	    commitHash = ObjectHash();
	    return c;
	}

	if (dstRepo->getHead().isEmpty()) {
	    fdtree = Tree::Flat();
	} else {
	    tip = dstRepo->getCommit(dstRepo->getHead());
	    Tree dtree = dstRepo->getTree(tip.getTree());
	    fdtree = dtree.flattened(dstRepo);
	}

	if (objEntry.isTree()) {
	    Tree stree = srcRepo->getTree(objEntry.hash);
	    Tree::Flat fstree = stree.flattened(srcRepo);

	    for (Tree::Flat::iterator it = fstree.begin();
		 it != fstree.end();
		 it++)
	    {
		fdtree[dstPath + (*it).first] = (*it).second;
	    }
	} else {
	    // XXX: Ideally we should prevent this commit
	    if (objEntry.type != TreeEntry::Null)
		fdtree[dstPath] = objEntry;
	}

        for (set<ObjectHash>::iterator it = objs.begin();
	     it != objs.end();
	     it++)
        {
	    if (!dstRepo->hasObject(*it)) {
		// XXX: Copy object without loading it all into memory!
	        dstRepo->addBlob(srcRepo->getObjectType(*it),
				 srcRepo->getPayload(*it));
	    }
        }

	Tree commitTree = Tree::unflatten(fdtree, dstRepo);

	c.setMessage(commit.getMessage());
	c.setUser(commit.getUser());
	c.setTime(commit.getTime());
	c.setGraft(srcRepo->getRootPath(), srcPath, hash);
	c.setParents(pFirst, pSecond);
	c.setTree(commitTree.hash());

	commitHash = dstRepo->addCommit(c);

	return c;
    }
private:
    // Source references
    ObjectHash hash;
    Commit commit;
    TreeEntry objEntry;
    set<ObjectHash> objs;
    // Destination references
    ObjectHash pFirst;
    ObjectHash pSecond;
    ObjectHash commitHash;
};

class GraftMapper : public DAGMapCB<ObjectHash, Commit, GraftDAGObject>
{
public:
    GraftMapper(LocalRepo *dstRepo, LocalRepo *srcRepo, const string &path)
    {
        dst = dstRepo;
        src = srcRepo;
        srcPath = path;
    }
    ~GraftMapper()
    {
    }
    virtual GraftDAGObject map(ObjectHash k, Commit v)
    {
	bool success;
	GraftDAGObject r = GraftDAGObject(k, v);

	if (!k.isEmpty()) {
	    success = r.setPath(srcPath, src);
	    ASSERT(success == true);
	}

	return r;
    }
private:
    string srcPath;
    LocalRepo *src;
    LocalRepo *dst;
};

/*
 * Graft a subtree from Repo 'r' to this repository.
 */
ObjectHash
LocalRepo::graftSubtree(LocalRepo *r,
                   const std::string &srcPath,
                   const std::string &dstPath)
{
    GraftMapper f = GraftMapper(this, r, srcPath);
    DAG<ObjectHash, Commit> cDag = r->getCommitDag();
    DAG<ObjectHash, GraftDAGObject> gDag = DAG<ObjectHash, GraftDAGObject>();

    gDag.graphMap(f, cDag);

    if (gDag.getNode(r->getHead()).isEmpty()) {
	return ObjectHash();
    }

    // XXX: Prune graft nodes

    // Graft individual changes
    ObjectHash tip = r->getHead();
    std::list<ObjectHash> bu = gDag.getBottomUp(tip);
    std::list<ObjectHash>::iterator it;
    for (it = bu.begin(); it != bu.end(); it++)
    {
	cout << "Grafting " << (*it).hex() << endl;

	// Compute and set parents
	tr1::unordered_set<ObjectHash> parents = gDag.getParents(*it);
	tr1::unordered_set<ObjectHash>::iterator p;

	p = parents.begin();
	if (parents.size() == 0) {
	    gDag.getNode(*it).setParents();
	} else if (parents.size() == 1) {
	    ObjectHash p1 = gDag.getNode(*p).getHash();
	    if (p1.isEmpty())
		p1 = getHead();
	    gDag.getNode(*it).setParents(p1);
	} else if (parents.size() == 2) {
	    ObjectHash p1 = gDag.getNode(*p).getHash();
	    if (p1.isEmpty())
		p1 = getHead();
	    p++;
	    ObjectHash p2 = gDag.getNode(*p).getHash();
	    if (p2.isEmpty())
		p2 = getHead();
	    gDag.getNode(*it).setParents(p1, p2);
	} else {
	    NOT_IMPLEMENTED(false);
	}

	Commit c = gDag.getNode(*it).graft(r, this, srcPath, dstPath);

	if (!gDag.getNode(*it).isEmpty())
	{
	    // Backrefs
	    MdTransaction::sp tr(metadata.begin());
	    addCommitBackrefs(c, tr);
	    tr->setMeta(c.hash(), "status", "graft");
	}
    }

    GraftDAGObject& gTip = gDag.getNode(tip);

    return gTip.getHash();
}

/*
 * Working Directory Operations
 */

int
listBranchesHelper(set<string> *rval, const string &path)
{
    string name = path.substr(path.find_last_of("/") + 1);
    rval->insert(name);

    return 0;
}

set<string>
LocalRepo::listBranches()
{
    string path = rootPath + ORI_PATH_HEADS;
    set<string> rval;

    DirIterate(path.c_str(), &rval, listBranchesHelper);

    return rval;
}

string
LocalRepo::getBranch()
{
    std::string branch = OriFile_ReadFile(rootPath + ORI_PATH_HEAD);
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

	OriFile_WriteFile(head.hex(), branchFile);
    }

    string ref = "@" + name;
    OriFile_WriteFile(ref.c_str(), ref.size(), rootPath + ORI_PATH_HEAD);
}

/*
 * Get the working repository version.
 */
ObjectHash
LocalRepo::getHead()
{
    string headPath;
    string branch = getBranch();
    string commitId;

    if (branch[0] == '@') {
	headPath = rootPath + ORI_PATH_HEADS + branch.substr(1);
        try {
            commitId = OriFile_ReadFile(headPath);
        } catch (std::ios_base::failure &e) {
	    return EMPTY_COMMIT;
        }
    } else if (branch[0] == '#') {
	commitId = branch.substr(1);
    } else {
	NOT_IMPLEMENTED(false);
    }

    return ObjectHash::fromHex(commitId);
}

/*
 * Update the working directory revision.
 */
void
LocalRepo::updateHead(const ObjectHash &commitId)
{
    string branch = getBranch();
    string headPath;

    ASSERT(!commitId.isEmpty());

    if (branch[0] == '@') {
	headPath = rootPath + ORI_PATH_HEADS + branch.substr(1);
	OriFile_WriteFile(commitId.hex(), headPath);
    } else if (branch[0] == '#') {
	string ref = "#" + commitId.hex();
	OriFile_WriteFile(ref.c_str(), ref.size(), rootPath + ORI_PATH_HEAD);
    } else {
	NOT_IMPLEMENTED(false);
    }
}

/*
 * Set working directory revision, this will set based on a commit's ObjectHash 
 * rather than updating the current head after a commit.  The command line 
 * checkout command should be the only way to call this.
 */
void
LocalRepo::setHead(const ObjectHash &commitId)
{
    string ref = "#" + commitId.hex();
    OriFile_WriteFile(ref.c_str(), ref.size(), rootPath + ORI_PATH_HEAD);
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
 * Set merge state
 */
void
LocalRepo::setMergeState(const MergeState &state)
{
    string mergeStatePath = rootPath + ORI_PATH_MERGESTATE;
    string blob = state.getBlob();

    OriFile_WriteFile(blob.data(), blob.size(), mergeStatePath);
}

/*
 * Get merge state
 */
MergeState
LocalRepo::getMergeState()
{
    string mergeStatePath = rootPath + ORI_PATH_MERGESTATE;
    MergeState state;
    string blob;

    blob = OriFile_ReadFile(mergeStatePath);
    state.fromBlob(blob);

    return state;
}

/*
 * Clear merge state
 */
void
LocalRepo::clearMergeState()
{
    string mergeStatePath = rootPath + ORI_PATH_MERGESTATE;

    OriFile_Delete(mergeStatePath);
}

/*
 * Has merge state?
 */
bool
LocalRepo::hasMergeState()
{
    string mergeStatePath = rootPath + ORI_PATH_MERGESTATE;

    return OriFile_Exists(mergeStatePath);
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
    // XXX: Excessive logging
    // printf("Temporary directory at %s\n", templ);
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
	OriFile_Delete(rootPath + ORI_PATH_REMOTES + name);
    }

    return true;
}

void
LocalRepo::setInstaClone(const std::string &name, bool val)
{
    map<string, Peer>::iterator it = peers.find(name);

    if (it != peers.end()) {
	(*it).second.setInstaClone(val);
    }
}

/*
 * Key Management
 */

PrivateKey
LocalRepo::getPrivateKey()
{
    PrivateKey key;

    key.open(rootPath + ORI_PATH_PRIVATEKEY);

    return key;
}

int
publicKeyCB(map<string, PublicKey> *keys, const string &path)
{
    PublicKey key = PublicKey();

    key.open(path);

    (*keys)[key.computeDigest()] = key;

    return 0;
}

map<string, PublicKey>
LocalRepo::getPublicKeys()
{
    string keyDir = rootPath + ORI_PATH_TRUSTED;
    map<string, PublicKey> keys;

    DirIterate(keyDir.c_str(), &keys, publicKeyCB);

    return keys;
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

