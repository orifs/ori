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
#include <cassert>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <iostream>

#include <openssl/sha.h>

#include "debug.h"
#include "util.h"
#include "object.h"
#include "httpclient.h"
#include "httprepo.h"
#include "packfile.h"

using namespace std;

/*
 * HttpRepo
 */

HttpRepo::HttpRepo(HttpClient *client)
    : client(client)
{
}

HttpRepo::~HttpRepo()
{
}

void
HttpRepo::preload(const std::vector<std::string> &objs)
{
    // TODO
    return;
}

std::string
HttpRepo::getUUID()
{
    int status;
    string uuid;

    status = client->getRequest("/id", uuid);
    if (status < 0) {
        assert(false);
        return "";
    }

    return uuid;
}

ObjectHash
HttpRepo::getHead()
{
    int status;
    string headId;

    status = client->getRequest("/HEAD", headId);
    if (status < 0) {
        assert(false);
        return ObjectHash();
    }

    return ObjectHash::fromHex(headId);
}

Object::sp
HttpRepo::getObject(const ObjectHash &id)
{
    LOG("WARNING: single ID request %s", id.hex().c_str());
    Util_PrintBacktrace();

    ObjectHashVec objs;
    objs.push_back(id);
    bytestream::ap bs(getObjects(objs));
    if (bs.get()) {
        numobjs_t num = bs->readInt<numobjs_t>();
        assert(num == 1);

        std::string info_str(ObjectInfo::SIZE, '\0');
        bs->readExact((uint8_t*)&info_str[0], ObjectInfo::SIZE);
        ObjectInfo info;
        info.fromString(info_str);

        uint32_t objSize = bs->readInt<uint32_t>();
        std::string payload(objSize, '\0');
        bs->readExact((uint8_t*)&payload[0], objSize);

        num = bs->readInt<numobjs_t>();
        assert(num == 0);

        if (info.getCompressed()) {
            payloads[info.hash] = lzmastream(new strstream(payload), DECOMPRESS,
                    info.payload_size).readAll();
        }
        else {
            payloads[info.hash] = payload;
        }
        return Object::sp(new HttpObject(this, info));
    }
    return Object::sp();
}

ObjectInfo
HttpRepo::getObjectInfo(const ObjectHash &id)
{
    int status;
    string payload;
    ObjectInfo rval = ObjectInfo();

    status = client->getRequest("/objinfo/" + id.hex(), payload);
    if (status < 0)
	return rval;

    rval.fromString(payload);

    return rval;
}

bool
HttpRepo::hasObject(const ObjectHash &id) {
    int status;
    string payload;

    status = client->getRequest("/objinfo/" + id.hex(), payload);

    return (status == 0);
}

bytestream *
HttpRepo::getObjects(const ObjectHashVec &vec) {
    strwstream ss;
    ss.writeInt<uint32_t>(vec.size());
    for (size_t i = 0; i < vec.size(); i++) {
        ss.writeHash(vec[i]);
    }

    string resp;
    int status = client->postRequest("/getobjs", ss.str(), resp);
    bytestream::ap bs(new strstream(resp));

    if (status == 0) {
        return bs.release();
    }
    return NULL;
}

std::set<ObjectInfo>
HttpRepo::listObjects()
{
    set<ObjectInfo> rval;

    string index;
    int status = client->getRequest("/index", index);
    strstream ss(index);

    if (status == 0) {
        uint64_t num = ss.readInt<uint64_t>();
        for (size_t i = 0; i < num; i++) {
            ObjectInfo info;
            ss.readInfo(info);
            rval.insert(info);
        }
    }
    else {
        assert(false);
    }

    return rval;
}

int
HttpRepo::addObject(ObjectType type, const ObjectHash &hash,
            const std::string &payload)
{
    NOT_IMPLEMENTED(false);
    return -1;
}

vector<Commit>
HttpRepo::listCommits()
{
    vector<Commit> rval;

    string index;
    int status = client->getRequest("/commits", index);
    strstream ss(index);

    if (status == 0) {
        uint32_t num = ss.readInt<uint32_t>();
        for (size_t i = 0; i < num; i++) {
            std::string commit_str;
            ss.readPStr(commit_str);
            Commit c;
            c.fromBlob(commit_str);
            rval.push_back(c);
        }
    }
    else {
        assert(false);
    }

    return rval;
}


std::string &
HttpRepo::_payload(const ObjectHash &id)
{
    return payloads[id];
}

void
HttpRepo::_addPayload(const ObjectHash &id, const std::string &payload)
{
    payloads[id] = payload;
}

void
HttpRepo::_clearPayload(const ObjectHash &id)
{
    payloads.erase(id);
}


/*
 * HttpObject
 */

HttpObject::HttpObject(HttpRepo *repo, ObjectInfo info)
    : Object(info), repo(repo)
{
    assert(repo != NULL);
    assert(!info.hash.isEmpty());
}

HttpObject::~HttpObject()
{
    repo->_clearPayload(info.hash);
}

bytestream *
HttpObject::getPayloadStream()
{
    return new strstream(repo->_payload(info.hash));
}

/*bytestream::ap
HttpObject::getStoredPayloadStream()
{
    return bytestream::ap(new strstream(repo->_payload(info.hash)));
}

size_t
HttpObject::getStoredPayloadSize()
{
    return repo->_payload(info.hash).length();
}*/
