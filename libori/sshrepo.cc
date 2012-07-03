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

#define D_READ 0
#define D_WRITE 1

// TODO: return char *s instead of string, return all values from same chunk of
// memory
class ResponseParser {
public:
    ResponseParser(const std::string &text) : text(text), off(0) {}

    bool ended() {
        return off >= text.size();
    }

    bool nextLine(std::string &line) {
        if (off >= text.size()) return false;

        size_t oldOff = off;
        while (off < text.size()) {
            if (text[off] == '\n') {
                off++;
                line = text.substr(oldOff, (off-oldOff-1));
                return true;
            }
            off++;
        }
        line = text.substr(oldOff, (off-oldOff-1));
        return true;
    }

    size_t getDataLength(const std::string &line) {
        size_t len = 0;
        if (sscanf(line.c_str(), "DATA %lu", &len) != 1) {
            return 0;
        }
        return len;
    }

    std::string getData(size_t len) {
        assert(off + len <= text.size());
        size_t oldOff = off;
        off += len;
        return text.substr(oldOff, len);
    }

    bool loadObjectInfo(ObjectInfo &info) {
        std::string line;
        nextLine(line); // hash
        if (line == "DONE") return false;

        assert(line.size() == 64); // 2*SHA256_DIGEST_LENGTH
        info.hash = line.data();

        nextLine(line); // type
        info.type = Object::getTypeForStr(line.c_str());
        assert(info.type != Object::Null);

        nextLine(line); // flags
        sscanf(line.c_str(), "%X", &info.flags);

        nextLine(line); // payload_size
        sscanf(line.c_str(), "%lu", &info.payload_size);
        assert(info.payload_size > 0);

        return true;
    }
private:
    const std::string &text;
    size_t off;
};

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

std::string SshRepo::getHead()
{
    client->sendCommand("show");
    std::string resp;
    client->recvResponse(resp);

    ResponseParser p(resp);
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

    ResponseParser p(resp);
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
    assert(false);
    return false;
}

std::set<ObjectInfo> SshRepo::listObjects()
{
    client->sendCommand("listobj");
    std::string resp;
    client->recvResponse(resp);

    std::set<ObjectInfo> rval;

    ResponseParser p(resp);
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
    assert(false);
    return -1;
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
