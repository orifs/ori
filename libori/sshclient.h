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

#ifndef __SSHCLIENT_H__
#define __SSHCLIENT_H__

#include <string>

#include "object.h"

class SshClient
{
public:
    SshClient(const std::string &remotePath);
    ~SshClient();

    int connect();
    void disconnect();
    bool connected();

    // At the moment the protocol is synchronous
    void sendCommand(const std::string &command);
    void recvResponse(std::string &out);

private:
    std::string remoteHost, remoteRepo;

    int fdFromChild, fdToChild;
    int childPid;
};

// TODO: return char *s instead of string, return all values from same chunk of
// memory
class SshResponseParser {
public:
    SshResponseParser(const std::string &text);

    bool ended() const;
    bool nextLine(std::string &line);
    size_t getDataLength(const std::string &line) const;
    std::string getData(size_t len);
    bool loadObjectInfo(ObjectInfo &info);

private:
    const std::string &text;
    size_t off;
};


#endif /* __SSHCLIENT_H__ */
