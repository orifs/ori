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

#include <cstdlib>
#include <cstdio>
#include <cassert>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>

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
HttpRepo::getHead()
{
    int status;
    string headId;

    status = client->getRequest("/HEAD", headId);
    if (status < 0) {
        assert(false);
        return "";
    }

    return headId;
}

Object::sp
HttpRepo::getObject(const std::string &id)
{
    int status;
    string index;
    ObjectInfo info = ObjectInfo(id.c_str());
    std::string payload;

    status = client->getRequest("/objs/" + id, payload);
    if (status < 0) {
        assert(false);
        return Object::sp();
    }

    info.setInfo(payload.substr(0, 16));

    // TODO: add raw data to cache
    _addPayload(info.hash, payload.substr(16));

    return Object::sp(new HttpObject(this, info));
}

bool
HttpRepo::hasObject(const std::string &id) {
    NOT_IMPLEMENTED(false);
    return false;
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
    for (int offset = 0; offset < index.size();)
    {
        string hash;
        string objInfo;
        ObjectInfo info;

        hash = index.substr(offset, SHA256_DIGEST_LENGTH * 2);
        offset += SHA256_DIGEST_LENGTH * 2;
        objInfo = index.substr(offset, 16);
        offset += 16;

        info = ObjectInfo(hash.c_str());
        info.setInfo(objInfo);
        rval.insert(info);
    }

    return rval;
}

int
HttpRepo::addObjectRaw(const ObjectInfo &info, bytestream *bs)
{
    NOT_IMPLEMENTED(false);
    return -1;
}

vector<Commit>
HttpRepo::listCommits()
{
    NOT_IMPLEMENTED(false);
    return vector<Commit>();
}


std::string &
HttpRepo::_payload(const std::string &id)
{
    return payloads[id];
}

void
HttpRepo::_addPayload(const std::string &id, const std::string &payload)
{
    payloads[id] = payload;
}

void
HttpRepo::_clearPayload(const std::string &id)
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
    assert(info.hash.size() > 0);
}

HttpObject::~HttpObject()
{
    repo->_clearPayload(info.hash);
}

bytestream::ap
HttpObject::getPayloadStream()
{
    if (info.getCompressed()) {
        bytestream::ap bs(getStoredPayloadStream());
        return bytestream::ap(new lzmastream(bs.release()));
    }
    return getStoredPayloadStream();
}

bytestream::ap
HttpObject::getStoredPayloadStream()
{
    return bytestream::ap(new strstream(repo->_payload(info.hash)));
}

size_t
HttpObject::getStoredPayloadSize()
{
    return repo->_payload(info.hash).length();
}
