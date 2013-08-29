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

#include <cstdlib>
#include <cstdio>

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include <sstream>
#include <deque>
#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <ori/packfile.h>
#include <ori/udsclient.h>
#include <ori/udsrepo.h>

using namespace std;
#ifdef HAVE_CXXTR1
using namespace std::tr1;
#endif /* HAVE_CXXTR1 */

/*
 * UDSRepo
 */

UDSRepo::UDSRepo()
    : client(NULL), containedObjs(NULL)
{
}

UDSRepo::UDSRepo(UDSClient *client)
    : client(client), containedObjs(NULL)
{
}

UDSRepo::~UDSRepo()
{
    if (containedObjs) {
        delete containedObjs;
    }
}

std::string UDSRepo::getUUID()
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

std::string UDSRepo::getVersion()
{
    client->sendCommand("get version");
    string version = "";

    bool ok = client->respIsOK();
    bytestream::ap bs(client->getStream());
    if (ok) {
        bs->readPStr(version);
    }
    return version;
}

ObjectHash UDSRepo::getHead()
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
UDSRepo::distance()
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

Object::sp UDSRepo::getObject(const ObjectHash &id)
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
        return Object::sp(new UDSObject(this, info));
    }
    return Object::sp();
}

bytestream *
UDSRepo::getObjects(const ObjectHashVec &objs)
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
UDSRepo::getObjectInfo(const ObjectHash &id)
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

bool UDSRepo::hasObject(const ObjectHash &id) {
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

std::set<ObjectInfo> UDSRepo::listObjects()
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
UDSRepo::addObject(ObjectType type, const ObjectHash &hash,
        const std::string &payload)
{
    NOT_IMPLEMENTED(false);
    return -1;
}

std::vector<Commit> UDSRepo::listCommits()
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

void
UDSRepo::transmit(bytewstream *out, const ObjectHashVec &objs)
{
    numobjs_t num;
    bytestream *in = getObjects(objs);
    vector<size_t> objSizes;
    string data;

    num = in->readUInt32();
    out->writeUInt32(num);
    while (num != 0) {
        objSizes.resize(num);

        for (numobjs_t i = 0; i < num; i++) {
            uint32_t objSize;
            string infoStr(ObjectInfo::SIZE, '\0');

            // Object info
            in->readExact((uint8_t *)&infoStr[0], ObjectInfo::SIZE);
            out->write((uint8_t *)&infoStr[0], ObjectInfo::SIZE);

            // Object size
            objSize = in->readUInt32();
            out->writeUInt32(objSize);
            objSizes[i] = objSize;
        }

        for (numobjs_t i = 0; i < num; i++) {
            uint32_t objSize = objSizes[i];
            data.resize(objSize);
            in->readExact((uint8_t *)&data[0], objSize);
            out->write((uint8_t *)&data[0], objSize);
        }

        num = in->readUInt32();
        out->writeUInt32(num);
    }

    return;
}

set<string>
UDSRepo::listExt()
{
    set<string> exts;
    client->sendCommand("ext list");

    bool ok = client->respIsOK();
    bytestream::ap bs(client->getStream());
    if (ok) {
        uint8_t numExts = bs->readUInt8();
        for (uint8_t i = 0; i < numExts; i++) {
            string ext;
            bs->readPStr(ext);
            exts.insert(ext);
        }
    }

    return exts;
}

string
UDSRepo::callExt(const string &ext, const string &data)
{
    client->sendCommand("ext call");

    strwstream ss;
    ss.writePStr(ext);
    ss.writeLPStr(data);
    client->sendData(ss.str());

    bool ok = client->respIsOK();
    bytestream::ap bs(client->getStream());
    if (ok) {
        string result;
        bs->readLPStr(result);
        return result;
    }

    return "";
}

std::string &UDSRepo::_payload(const ObjectHash &id)
{
    return payloads[id];
}

void UDSRepo::_addPayload(const ObjectHash &id, const std::string &payload)
{
    payloads[id] = payload;
}

void UDSRepo::_clearPayload(const ObjectHash &id)
{
    payloads.erase(id);
}


/*
 * UDSObject
 */

UDSObject::UDSObject(UDSRepo *repo, ObjectInfo info)
    : Object(info), repo(repo)
{
    ASSERT(repo != NULL);
    ASSERT(!info.hash.isEmpty());
}

UDSObject::~UDSObject()
{
    repo->_clearPayload(info.hash);
}

bytestream *UDSObject::getPayloadStream()
{
    return new strstream(repo->_payload(info.hash));
}
