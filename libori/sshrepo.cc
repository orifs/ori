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

#include "sshclient.h"
#include "sshrepo.h"
#include "util.h"
#include "debug.h"

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

void SshRepo::preload(const std::vector<std::string> &objs)
{
    // TODO
    return;

    client->sendCommand("readobjs " + objs.size());
    for (int i = 0; i < objs.size(); i++) {
        client->sendCommand(objs[i]);
    }
}

std::string SshRepo::getHead()
{
    client->sendCommand("show");
    std::string resp;
    client->recvResponse(resp);

    SshResponseParser p(resp);
    std::string line;
    p.nextLine(line); // root path
    p.nextLine(line); // uuid
    p.nextLine(line); // version
    p.nextLine(line); // head
    return line;
}

Object *SshRepo::getObject(const std::string &id)
{
    std::string command = "readobj " + id;
    client->sendCommand(command);

    std::string resp;
    client->recvResponse(resp);

    SshResponseParser p(resp);
    ObjectInfo info;
    p.loadObjectInfo(info);

    std::string line;
    p.nextLine(line); // DATA x
    std::string raw_data = p.getData(p.getDataLength(line));

    // TODO: add raw data to cache
    _addPayload(info.hash, raw_data);

    return new SshObject(this, info);
}

bool SshRepo::hasObject(const std::string &id) {
    NOT_IMPLEMENTED(false);
    return false;
}

std::set<ObjectInfo> SshRepo::listObjects()
{
    client->sendCommand("list objs");
    std::string resp;
    client->recvResponse(resp);

    std::set<ObjectInfo> rval;

    SshResponseParser p(resp);
    std::string line;
    while (!p.ended()) {
        ObjectInfo info;
        if (!p.loadObjectInfo(info)) break;
        rval.insert(info);
    }

    return rval;
}

int SshRepo::addObjectRaw(const ObjectInfo &info, bytestream *bs)
{
    NOT_IMPLEMENTED(false);
    return -1;
}

std::vector<Commit> SshRepo::listCommits()
{
    client->sendCommand("list commits");
    std::string resp;
    client->recvResponse(resp);

    std::vector<Commit> rval;

    SshResponseParser p(resp);
    std::string line;
    while (!p.ended()) {
        if (!p.nextLine(line)) break;
        size_t len = p.getDataLength(line);
        std::string data = p.getData(len);
        Commit c;
        c.fromBlob(data);
        rval.push_back(c);
    }

    return rval;
}



std::string &SshRepo::_payload(const std::string &id)
{
    return payloads[id];
}

void SshRepo::_addPayload(const std::string &id, const std::string &payload)
{
    payloads[id] = payload;
}

void SshRepo::_clearPayload(const std::string &id)
{
    payloads.erase(id);
}


/*
 * SshObject
 */

SshObject::SshObject(SshRepo *repo, ObjectInfo info)
    : Object(info), repo(repo)
{
    assert(repo != NULL);
    assert(info.hash.size() > 0);
}

SshObject::~SshObject()
{
    repo->_clearPayload(info.hash);
}

bytestream::ap SshObject::getPayloadStream()
{
    if (info.getCompressed()) {
        bytestream::ap bs(getStoredPayloadStream());
        return bytestream::ap(new lzmastream(bs.release()));
    }
    return getStoredPayloadStream();
}

bytestream::ap SshObject::getStoredPayloadStream()
{
    return bytestream::ap(new strstream(repo->_payload(info.hash)));
}

size_t SshObject::getStoredPayloadSize()
{
    return repo->_payload(info.hash).length();
}
