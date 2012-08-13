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
    int status;
    string index;
    ObjectInfo info = ObjectInfo(id);
    string payload;

    status = client->getRequest("/objs/" + id.hex(), payload);
    if (status < 0) {
        assert(false);
        return Object::sp();
    }

    info.fromString(payload.substr(0, 16));

    // TODO: add raw data to cache
    _addPayload(info.hash, payload.substr(16));

    return Object::sp(new HttpObject(this, info));
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
    NOT_IMPLEMENTED(false);
    return NULL;
}

std::set<ObjectInfo>
HttpRepo::listObjects()
{
    int status;
    string index;
    set<ObjectInfo> rval;

    status = client->getRequest("/index", index);
    if (status < 0) {
        assert(false);
        return rval;
    }

    // Parse response
    for (size_t offset = 0; offset < index.size();)
    {
        ObjectHash hash;
        string objInfo;
        ObjectInfo info;

        hash = ObjectHash::fromHex(index.substr(offset, ObjectHash::SIZE * 2));
        offset += ObjectHash::SIZE * 2;
        objInfo = index.substr(offset, 16);
        offset += 16;

        info = ObjectInfo(hash);
        info.fromString(objInfo);
        rval.insert(info);
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
    if (status < 0) {
        assert(false);
        return rval;
    }

    // Parse response
    for (size_t offset = 0; offset < index.size();)
    {
        int16_t bsize = *(int16_t*)&index[offset];
        bsize = ntohs(bsize);
        offset += sizeof(int16_t);

        string blob = index.substr(offset, bsize);
        offset += bsize;

        Commit c;
        c.fromBlob(blob);
        rval.push_back(c);
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
