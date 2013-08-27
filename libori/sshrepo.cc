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

#include <cstdlib>
#include <cstdio>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <sstream>
#include <deque>
#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <ori/packfile.h>
#include <ori/sshclient.h>
#include <ori/sshrepo.h>

using namespace std;
#ifdef HAVE_CXXTR1
using namespace std::tr1;
#endif /* HAVE_CXXTR1 */

/*
 * SshRepo
 */

SshRepo::SshRepo(SshClient *client)
    : client(client), containedObjs(NULL)
{
}

SshRepo::~SshRepo()
{
    if (containedObjs) {
        delete containedObjs;
    }
}

std::string SshRepo::getUUID()
{
    client->sendCommand("get fsid");
    string fsid = "";

    bool ok = client->respIsOK();
    bytestream::ap bs(client->getStream());
    if (ok) {
        bs->readPStr(fsid);
    }
    return fsid;
}

ObjectHash SshRepo::getHead()
{
    client->sendCommand("get head");
    ObjectHash hash;

    bool ok = client->respIsOK();
    bytestream::ap bs(client->getStream());
    if (ok) {
        bs->readHash(hash);
    }
    return hash;
}

int
SshRepo::distance()
{
    boost::posix_time::ptime t_begin =
        boost::posix_time::microsec_clock::local_time();

    // Send hello command
    client->sendCommand("hello");
    bool ok = client->respIsOK();
    assert(ok);
    bytestream::ap bs(client->getStream());
    std::string version;
    bs->readPStr(version);
    
    boost::posix_time::ptime t_end =
        boost::posix_time::microsec_clock::local_time();
    return (t_end - t_begin).total_milliseconds();
}

Object::sp SshRepo::getObject(const ObjectHash &id)
{
    ObjectHashVec objs;
    objs.push_back(id);
    bytestream::ap bs(getObjects(objs));
    if (bs.get()) {
        ASSERT(sizeof(numobjs_t) == sizeof(uint32_t));
        numobjs_t num = bs->readUInt32();
        ASSERT(num == 1);

        std::string info_str(ObjectInfo::SIZE, '\0');
        bs->readExact((uint8_t*)&info_str[0], ObjectInfo::SIZE);
        ObjectInfo info;
        info.fromString(info_str);

        uint32_t objSize = bs->readUInt32();
        std::string payload(objSize, '\0');
        bs->readExact((uint8_t*)&payload[0], objSize);

        ASSERT(sizeof(numobjs_t) == sizeof(uint32_t));
        num = bs->readUInt32();
        ASSERT(num == 0);

        switch (info.getAlgo()) {
            case ObjectInfo::ZIPALGO_NONE:
                payloads[info.hash] = payload;
                break;
            case ObjectInfo::ZIPALGO_FASTLZ:
                payloads[info.hash] = zipstream(new strstream(payload),
                                                DECOMPRESS,
                                                info.payload_size).readAll();
            case ObjectInfo::ZIPALGO_LZMA:
            case ObjectInfo::ZIPALGO_UNKNOWN:
                NOT_IMPLEMENTED(false);
        }
        return Object::sp(new SshObject(this, info));
    }
    return Object::sp();
}

bytestream *
SshRepo::getObjects(const ObjectHashVec &objs)
{
    client->sendCommand("readobjs");

    strwstream ss;
    ss.writeUInt32(objs.size());
    for (size_t i = 0; i < objs.size(); i++) {
        ss.writeHash(objs[i]);
    }
    client->sendData(ss.str());
    DLOG("Requesting %lu objects", objs.size());

    bool ok = client->respIsOK();
    bytestream::ap bs(client->getStream());
    if (ok) {
        return bs.release();
    }
    return NULL;
}

ObjectInfo
SshRepo::getObjectInfo(const ObjectHash &id)
{
    client->sendCommand("getobjinfo");

    strwstream ss;
    ss.writeHash(id);
    client->sendData(ss.str());

    bool ok = client->respIsOK();
    if (!ok) {
        return ObjectInfo();
    }

    bytestream::ap bs(client->getStream());
    ObjectInfo info;
    bs->readInfo(info);

    return info;
}

bool SshRepo::hasObject(const ObjectHash &id) {
    if (!containedObjs) {
        containedObjs = new unordered_set<ObjectHash>();
        std::set<ObjectInfo> objs = listObjects();
        for (std::set<ObjectInfo>::iterator it = objs.begin();
                it != objs.end();
                it++) {
            containedObjs->insert((*it).hash);
        }
    }

    return containedObjs->find(id) != containedObjs->end();
}

std::set<ObjectInfo> SshRepo::listObjects()
{
    client->sendCommand("list objs");
    std::set<ObjectInfo> rval;

    bool ok = client->respIsOK();
    bytestream::ap bs(client->getStream());
    if (ok) {
        uint64_t num = bs->readUInt64();
        for (size_t i = 0; i < num; i++) {
            ObjectInfo info;
            bs->readInfo(info);
            rval.insert(info);
        }
    }

    return rval;
}

int
SshRepo::addObject(ObjectType type, const ObjectHash &hash,
        const std::string &payload)
{
    NOT_IMPLEMENTED(false);
    return -1;
}

std::vector<Commit> SshRepo::listCommits()
{
    client->sendCommand("list commits");
    std::vector<Commit> rval;

    bool ok = client->respIsOK();
    bytestream::ap bs(client->getStream());
    if (ok) {
        uint32_t num = bs->readUInt32();
        for (size_t i = 0; i < num; i++) {
            std::string commit_str;
            bs->readPStr(commit_str);
            Commit c;
            c.fromBlob(commit_str);
            rval.push_back(c);
        }
    }

    return rval;
}



std::string &SshRepo::_payload(const ObjectHash &id)
{
    return payloads[id];
}

void SshRepo::_addPayload(const ObjectHash &id, const std::string &payload)
{
    payloads[id] = payload;
}

void SshRepo::_clearPayload(const ObjectHash &id)
{
    payloads.erase(id);
}


/*
 * SshObject
 */

SshObject::SshObject(SshRepo *repo, ObjectInfo info)
    : Object(info), repo(repo)
{
    ASSERT(repo != NULL);
    ASSERT(!info.hash.isEmpty());
}

SshObject::~SshObject()
{
    repo->_clearPayload(info.hash);
}

bytestream *SshObject::getPayloadStream()
{
    return new strstream(repo->_payload(info.hash));
}
