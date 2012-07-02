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

    bool nextLine(std::string &line) {
        if (off >= text.size()) return false;

        size_t oldOff = off;
        while (off < text.size()) {
            if (text[off] == '\n') {
                off++;
                line = text.substr(oldOff, (off-oldOff));
                return true;
            }
            off++;
        }
        line = text.substr(oldOff, (off-oldOff));
        return true;
    }

    size_t getDataLength(const std::string &line) {
        size_t len = 0;
        if (sscanf(line.c_str(), "DATA %lu\n", &len) != 1) {
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
    return line.substr(0, line.size()-1);
}

int SshRepo::getObjectRaw(Object::ObjectInfo *info, std::string &raw_data)
{
    std::string command = "readobj ";
    command += info->hash;
    client->sendCommand(command);

    std::string resp;
    client->recvResponse(resp);

    ResponseParser p(resp);
    std::string line;
    p.nextLine(line); // hash

    p.nextLine(line); // type
    line[line.size()-1] = '\0';
    info->type = Object::getTypeForStr(line.c_str());

    p.nextLine(line); // flags
    sscanf(line.c_str(), "%X\n", &info->flags);

    p.nextLine(line); // payload_size
    sscanf(line.c_str(), "%lu\n", &info->payload_size);

    p.nextLine(line); // DATA x
    raw_data = p.getData(p.getDataLength(line));

    return 0;
}

std::set<std::string> SshRepo::listObjects()
{
    client->sendCommand("listobj");
    std::string resp;
    client->recvResponse(resp);

    std::set<std::string> rval;

    ResponseParser p(resp);
    std::string line;
    while (p.nextLine(line)) {
        if (line != "DONE\n")
            rval.insert(line.substr(0, line.size()-1));
    }

    return rval;
}

Object SshRepo::addObjectRaw(const Object::ObjectInfo &info, const std::string
        &raw_data)
{
    assert(false);
    return Object();
}

Object SshRepo::addObjectRaw(Object::ObjectInfo info, bytestream *bs)
{
    assert(false);
    return Object();
}

std::string SshRepo::addObject(Object::ObjectInfo info, const std::string
        &data)
{
    assert(false);
    return "";
}

