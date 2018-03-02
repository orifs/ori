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

#ifndef __ORISYNCCLIENT_H__
#define __ORISYNCCLIENT_H__

#include <string>

#include <oriutil/stream.h>
#include "object.h"

class OrisyncClient
{
public:
    OrisyncClient();
    OrisyncClient(const std::string &remotePath);
    ~OrisyncClient();

    int connect();
    void disconnect();
    bool connected();

    // At the moment the protocol is synchronous
    void sendData(const std::string &data);
    bytestream *getStream();

    int callCmd(const string &cmd, const string &data);

    bool respIsOK();

private:
    std::string orisyncPath, remoteRepo;

    int fd;
    bytewstream::ap streamToChild;
};


#endif /* __ORISYNCCLIENT_H__ */
