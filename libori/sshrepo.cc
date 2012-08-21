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

#include "debug.h"
#include "util.h"
#include "packfile.h"
#include "sshclient.h"
#include "sshrepo.h"

/*
 * SshRepo
 */

SshRepo::SshRepo(SshClient *client)
    : client(client)
{
}

SshRepo::~SshRepo()
{
}

std::string SshRepo::getUUID()
{
    // XXX: Implement me!
    return "";
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
        numobjs_t num = bs->readInt<numobjs_t>();
        ASSERT(num == 1);

        std::string info_str(ObjectInfo::SIZE, '\0');
        bs->readExact((uint8_t*)&info_str[0], ObjectInfo::SIZE);
        ObjectInfo info;
        info.fromString(info_str);

        uint32_t objSize = bs->readInt<uint32_t>();
        std::string payload(objSize, '\0');
        bs->readExact((uint8_t*)&payload[0], objSize);

        num = bs->readInt<numobjs_t>();
        ASSERT(num == 0);

        if (info.getCompressed()) {
            payloads[info.hash] = zipstream(new strstream(payload), DECOMPRESS,
                    info.payload_size).readAll();
        }
        else {
            payloads[info.hash] = payload;
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
    ss.writeInt<uint32_t>(objs.size());
    for (size_t i = 0; i < objs.size(); i++) {
        ss.writeHash(objs[i]);
    }
    client->sendData(ss.str());
    fprintf(stderr, "Requesting %lu objects\n", objs.size());

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
    NOT_IMPLEMENTED(false);
    return ObjectInfo();
}

bool SshRepo::hasObject(const ObjectHash &id) {
    NOT_IMPLEMENTED(false);
    return false;
}

std::set<ObjectInfo> SshRepo::listObjects()
{
    client->sendCommand("list objs");
    std::set<ObjectInfo> rval;

    bool ok = client->respIsOK();
    bytestream::ap bs(client->getStream());
    if (ok) {
        uint64_t num = bs->readInt<uint64_t>();
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
        uint32_t num = bs->readInt<uint32_t>();
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
