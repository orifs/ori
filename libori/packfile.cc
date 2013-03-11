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


#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <string>
#include <set>
#include <list>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <boost/tr1/unordered_set.hpp>

#include "tuneables.h"

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/scan.h>
#include <oriutil/posixexception.h>
#include <ori/packfile.h>
#include <ori/index.h>

using namespace std;

PfTransaction::PfTransaction(Packfile *pf, Index *idx)
    : totalSize(0), committed(false), pf(pf), idx(idx)
{
}

PfTransaction::~PfTransaction()
{
    if (!committed)
        commit();
}

bool PfTransaction::full() const
{
    return infos.size() >= PACKFILE_MAXOBJS ||
        totalSize >= PACKFILE_MAXSIZE;
}

float
PfTransaction::_checkCompressionRatio(const string &payload)
{
    return 1.5f;
}

void
PfTransaction::addPayload(ObjectInfo info, const string &payload)
{
    if (committed) {
        throw runtime_error("Adding payload to already-committed transaction!");
    }

#if DEBUG
    for (size_t i = 0; i < infos.size(); i++) {
        if (infos[i].hash == info.hash) {
            fprintf(stderr, "WARNING: duplicate addPayload %s!\n",
                    info.hash.hex().c_str());
            info.print(cerr);
        }
    }
#endif

#ifdef ENABLE_COMPRESSION
    zipstream ls(new strstream(payload), COMPRESS);
    uint8_t buf[COMPCHECK_BYTES];
    size_t compSize = 0;
    bool compress = false;

    if (payload.size() > ZIP_MINIMUM_SIZE) {
        compSize = ls.read(buf, COMPCHECK_BYTES);
        float ratio = (float)compSize / (float)ls.inputConsumed();

        if (ratio <= COMPCHECK_RATIO) {
            compress = true;
        }
    }

    if (compress) {
        // Okay to compress
        info.flags |= ORI_FLAG_COMPRESSED;
        // Reuse compression test data
        strwstream ss(string((char*)buf, compSize));
        ss.copyFrom(&ls);
        
        payloads.push_back(ss.str());
        totalSize += ss.str().size();
    }
    else
#endif
    {
        payloads.push_back(payload);
        totalSize += payload.size();
    }

    infos.push_back(info);
    hashToIx[info.hash] = infos.size()-1;
}

bool PfTransaction::has(const ObjectHash &hash) const
{
    return hashToIx.find(hash) != hashToIx.end();
}

void PfTransaction::commit()
{
    pf->commit(this, idx);
    if (!committed) {
        throw runtime_error("Unknown error committing PfTransaction");
    }
}


// stored length + offset
#define ENTRYSIZE (ObjectInfo::SIZE + 4 + 4)

Packfile::Packfile(const string &filename, packid_t id)
    : fd(-1), filename(filename), packid(id), numObjects(0), fileSize(0)
{
    fd = ::open(filename.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        perror("Packfile open");
        throw PosixException(errno);
    }

    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        perror("Packfile fstat");
        throw PosixException(errno);
    }

    fileSize = sb.st_size;
    
    // TODO: check for corruption?
}

Packfile::~Packfile()
{
    if (fd > 0)
        close(fd);
}

bool Packfile::full() const
{
    return numObjects >= PACKFILE_MAXOBJS ||
        fileSize >= PACKFILE_MAXSIZE;
}

PfTransaction::sp
Packfile::begin(Index *idx)
{
    return PfTransaction::sp(new PfTransaction(this, idx));
}

void
Packfile::commit(PfTransaction *t, Index *idx)
{
    if (t->infos.size() != t->payloads.size()) {
        throw runtime_error("PfTransaction infos.size() != payloads.size())");
    }

    lseek(fd, 0, SEEK_END);
    vector<offset_t> offsets;
    size_t headers_size = t->infos.size() * ENTRYSIZE;
    offset_t off = fileSize + sizeof(numobjs_t) + headers_size;
    
    strwstream headers_ss;
    ASSERT(sizeof(numobjs_t) == sizeof(uint32_t));
    headers_ss.writeUInt32(t->infos.size());
    for (size_t i = 0; i < t->infos.size(); i++) {
        headers_ss.write(t->infos[i].toString().data(), ObjectInfo::SIZE);
        headers_ss.writeUInt32(t->payloads[i].size());
        ASSERT(sizeof(uint32_t) == sizeof(offset_t));
        headers_ss.writeUInt32(off);

        offsets.push_back(off);
        off += t->payloads[i].size();
    }

    write(fd, headers_ss.str().data(), headers_ss.str().size());
    fileSize += headers_ss.str().size();

    for (size_t i = 0; i < t->payloads.size(); i++) {
        write(fd, t->payloads[i].data(), t->payloads[i].size());
        fileSize += t->payloads[i].size();
        numObjects++;

        IndexEntry ie;
	ie.info = t->infos[i];
	ie.offset = offsets[i];
	ie.packed_size = t->payloads[i].size();
	ie.packfile = packid;

        idx->updateEntry(ie.info.hash, ie);
    }

    t->committed = true;
}

bytestream *Packfile::getPayload(const IndexEntry &entry)
{
    ASSERT(entry.packfile == packid);
    bytestream *stored = new fdstream(fd, entry.offset, entry.packed_size);
#ifndef ENABLE_COMPRESSION
    assert(!entry.info.getCompressed());
    return stored;
#else
    if (!entry.info.getCompressed()) {
        return stored;
    }
    return new zipstream(stored, DECOMPRESS, entry.info.payload_size);
#endif
}

bool Packfile::purge(const set<ObjectHash> &hset, Index *idx)
{
    PfTransaction::sp tr = begin(idx);
    
    // Read the current contents
    lseek(fd, 0, SEEK_SET);
    vector<uint32_t> storedSizes;

    fdstream fs(fd, 0);
    while (!fs.ended()) {
        string payload;
        set<size_t> skip;

        numobjs_t num;
        try {
            num = fs.readUInt32();
        }
        catch (ios_base::failure &e) {
            break;
        }

        storedSizes.resize(num);

        // Read headers
        for (size_t i = 0; i < num; i++) {
            ObjectInfo info;
            fs.readInfo(info);
            uint32_t ssize = fs.readUInt32();
            offset_t off = fs.readUInt32();
            (void)off;

            storedSizes[i] = ssize;

            if (hset.find(info.hash) != hset.end()) {
                skip.insert(i);
            }
            else {
                tr->infos.push_back(info);
            }
        }

        // Read payloads
        for (size_t i = 0; i < num; i++) {
            payload.resize(storedSizes[i]);
            fs.read((uint8_t*)&payload[0], storedSizes[i]);

            if (skip.find(i) != skip.end()) {
                continue;
            }

            tr->payloads.push_back(payload);
        }
    }

    // Make a tempfile
    string tmpFilename = filename + ".tmp";
    int oldFd = fd;
    fd = ::open(tmpFilename.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        perror("Packfile::purge open");
        throw PosixException(errno);
    }

    ::close(oldFd);
    Util_RenameFile(tmpFilename, filename);

    // Commit the transaction
    bool empty = tr->payloads.size() == 0;
    tr.reset();

    return empty;
}

void
Packfile::readEntries(ReadEntryCb cb, void *arg)
{
    offset_t groupOffset = 0;
    
    while (groupOffset < fileSize) {
        fdstream readStream(fd, groupOffset);
        numobjs_t objs = readStream.readUInt32();

        for (size_t i = 0; i < objs; i++) {
            ObjectInfo info;
            uint32_t size;
            offset_t off;

            ASSERT(sizeof(offset_t) == sizeof(uint32_t));

            readStream.readInfo(info);
            size = readStream.readUInt32();
            off = readStream.readUInt32();
            cb(info, off, arg);

            ASSERT(groupOffset <= size + off);
            groupOffset = size + off;
        }
    }
}

bool
_offsetCmp(const IndexEntry &ie1, const IndexEntry &ie2)
{
    return ie1.offset < ie2.offset;
}

void
Packfile::transmit(bytewstream *bs, vector<IndexEntry> objects)
{
    // Find contiguous blocks
    sort(objects.begin(), objects.end(), _offsetCmp);
    map<offset_t, offset_t> blocks;
    for (size_t i = 0; i < objects.size(); i++) {
        offset_t offset = objects[i].offset;
        offset_t off_end = offset + objects[i].packed_size;
        if (objects[i].packed_size == 0) {
            // Empty objects
            continue;
        }

        if (blocks.size() == 0) {
            blocks[offset] = off_end;
        }
        else {
            map<offset_t, offset_t>::iterator it = blocks.upper_bound(offset);
            if (it == blocks.begin()) {
                blocks[offset] = off_end;
            }
            else {
                it--;
                if ((*it).second == offset) {
                    offset = (*it).first;
                    blocks[offset] = off_end;
                }
                else {
                    blocks[offset] = off_end;
                }
            }
            
            // Fix the rest of the array
            while (blocks.find(off_end) != blocks.end()) {
                offset_t off_end_old = off_end;
                off_end = blocks[off_end_old];
                blocks.erase(off_end_old);
                blocks[offset] = off_end;
            }
        }
    }

    // Transmit object infos
    tr1::unordered_set<ObjectHash> includedHashes;
    numobjs_t totalObjs = 0;

    strwstream infos_ss;
    for (size_t i = 0; i < objects.size(); i++) {
        if (includedHashes.find(objects[i].info.hash) != includedHashes.end()) {
            // Duplicate object
            fprintf(stderr, "WARNING packfile: duplicate object in transmit\n");
            continue;
        }
        includedHashes.insert(objects[i].info.hash);
        totalObjs++;

        string info_str = objects[i].info.toString();
        infos_ss.write(info_str.data(), info_str.size());
        infos_ss.writeUInt32(objects[i].packed_size);

        //fprintf(stderr, "Obj %lu packed size %u\n", i, objects[i].packed_size);
    }

    ASSERT(sizeof(numobjs_t) == sizeof(uint32_t));
    bs->writeUInt32(totalObjs);
    bs->write(infos_ss.str().data(), infos_ss.str().size());

    // Transmit objects
    vector<uint8_t> buf;
    for (map<offset_t, offset_t>::iterator it = blocks.begin();
            it != blocks.end();
            it++) {
        lseek(fd, (*it).first, SEEK_SET);
	ASSERT((*it).second >= (*it).first);
        ssize_t len = (*it).second - (*it).first;
        buf.resize(len);
        ssize_t n = read(fd, &buf[0], len);
        if (n < 0 || n != len) {
            throw PosixException(errno);
        }
        ASSERT(n == len);
        //fprintf(stderr, "Wrote block size %ld\n", len);

        bs->write(&buf[0], len);
    }
    ASSERT(!bs->error());
}


bool
Packfile::receive(bytestream *bs, Index *idx)
{
    ASSERT(sizeof(uint32_t) == sizeof(numobjs_t));
    numobjs_t num = bs->readUInt32();
    if (num == 0) return false;

    lseek(fd, 0, SEEK_END);
    size_t headers_size = num * ENTRYSIZE;
    offset_t off = fileSize + sizeof(numobjs_t) + headers_size;
    vector<size_t> obj_sizes;
    
    strwstream headers_ss;
    ASSERT(sizeof(offset_t) == sizeof(numobjs_t));
    headers_ss.writeUInt32(num);
    for (size_t i = 0; i < num; i++) {
        string info_str(ObjectInfo::SIZE, '\0');
        bs->readExact((uint8_t*)&info_str[0], ObjectInfo::SIZE);
        ObjectInfo info;
        info.fromString(info_str);
        //info.print();

        uint32_t obj_size = bs->readUInt32();
        obj_sizes.push_back(obj_size);

        headers_ss.write(info_str.data(), ObjectInfo::SIZE);
        headers_ss.writeUInt32(obj_size);
        ASSERT(sizeof(offset_t) == sizeof(uint32_t));
        headers_ss.writeUInt32(off);

        IndexEntry ie = {info, off, obj_size, packid};
        idx->updateEntry(info.hash, ie);

        off += obj_size;
    }

    write(fd, headers_ss.str().data(), headers_ss.str().size());
    fileSize += headers_ss.str().size();

    vector<uint8_t> data;
    for (size_t i = 0; i < num; i++) {
        //fprintf(stderr, "Reading %lu packed size %lu\n", i, obj_sizes[i]);
        data.resize(obj_sizes[i]);
        bs->readExact(&data[0], obj_sizes[i]);
        
        write(fd, &data[0], obj_sizes[i]);
        fileSize += obj_sizes[i];
        numObjects++;
    }

    return true;
}




/*
 * PackfileManager
 */

PackfileManager::PackfileManager(const string &rootPath)
    : rootPath(rootPath)
{
    if (!_loadFreeList()) {
        _recomputeFreeList();
        _writeFreeList();
    }
}

PackfileManager::~PackfileManager()
{
    _writeFreeList();
}

Packfile::sp
PackfileManager::getPackfile(packid_t id)
{
    if (!_packfileCache.hasKey(id)) {
        Packfile::sp pf(new Packfile(_getPackfileName(id), id));

        _packfileCache.put(id, pf);
        return pf;
    }

    return _packfileCache.get(id);
}

Packfile::sp
PackfileManager::newPackfile()
{
    ASSERT(freeList.size() > 0);
    packid_t id = freeList[0];
    Packfile::sp pf(new Packfile(_getPackfileName(id), id));
    if (freeList.size() == 1) {
        freeList[0] += 1;
    }
    else {
        freeList.pop_front();
    }
    return pf;
}

bool
PackfileManager::hasPackfile(packid_t id)
{
    return Util_FileExists(_getPackfileName(id));
}

static int _freeListCB(vector<packid_t> *existing, const string &cpath)
{
    string path = StrUtil_Basename(cpath);
    packid_t id = 0;
    if (sscanf(path.c_str(), "pack%u.pak", &id) != 1) {
        return 0;
    }
    existing->push_back(id);
    return 0;
}

vector<packid_t>
PackfileManager::getPackfileList()
{
    vector<packid_t> existing;

    DirIterate(rootPath.c_str(), &existing, _freeListCB);

    return existing;
}

void
PackfileManager::_recomputeFreeList()
{
    vector<packid_t> existing;
    DirIterate(rootPath.c_str(), &existing, _freeListCB);
    sort(existing.begin(), existing.end());

    freeList.clear();

    if (existing.size() > 0) {
        packid_t curr = 0;
        size_t ix = 0;
        while (curr < existing.back()) {
            while (curr < existing[ix]) {
                freeList.push_back(curr);
                curr++;
            }
            curr++;
            ix++;
        }
        freeList.push_back(existing.back()+1);
    }
    else {
        freeList.push_back(0);
    }
}

bool
PackfileManager::_loadFreeList()
{
    string freeListPath = rootPath + PFMGR_FREELIST;
    int fd = ::open(freeListPath.c_str(), O_RDONLY);
    if (fd < 0) {
        //perror("PackfileManager::_loadFreeList open");
        return false;
    }

    freeList.clear();

    fdstream fs(fd, 0);
    uint32_t numEntries = fs.readUInt32();
    if (fs.error()) return false;
    for (size_t i = 0; i < numEntries; i++) {
        ASSERT(sizeof(packid_t) == sizeof(uint32_t));
        packid_t id = fs.readUInt32();
        if (fs.error()) return false;
        freeList.push_back(id);
    }

    close(fd);

    return true;
}

void
PackfileManager::_writeFreeList()
{
    strwstream ss;
    ss.writeUInt32(freeList.size());
    for (size_t i = 0; i < freeList.size(); i++) {
        packid_t id = freeList[i];
        ASSERT(sizeof(uint32_t) == sizeof(packid_t));
        ss.writeUInt32(id);
    }

    string freeListPath = rootPath + PFMGR_FREELIST;
    int fd = ::open(freeListPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("PackfileManager::_loadFreeList open");
        throw PosixException(errno);
    }
    const string &str = ss.str();
    write(fd, str.data(), str.size());
    close(fd);
}

string
PackfileManager::_getPackfileName(packid_t id)
{
    stringstream ss;
    ss << rootPath;
    ss << "pack";
    ss << id;
    ss << ".pak";
    return ss.str();
}



int cmd_testpackfiles(int argc, char *argv[])
{
    return 0;
}
