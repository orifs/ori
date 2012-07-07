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

#include "httpclient.h"
#include "httprepo.h"
#include "util.h"
#include "debug.h"

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
    string line;

    return line;
}

Object *
HttpRepo::getObject(const std::string &id)
{
    ObjectInfo info;
    std::string raw_data;

    // TODO: add raw data to cache
    _addPayload(info.hash, raw_data);

    return new HttpObject(this, info);
}

bool
HttpRepo::hasObject(const std::string &id) {
    NOT_IMPLEMENTED(false);
    return false;
}

std::set<ObjectInfo>
HttpRepo::listObjects()
{
    std::set<ObjectInfo> rval;

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
