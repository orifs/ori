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

#ifndef __SERVER_H__
#define __SERVER_H__

#include <oriutil/mutex.h>

#define ORI_UDS_PROTO_VERSION "1.0"

class UDSSession;

class UDSServer : public Thread
{
public:
    UDSServer(LocalRepo *repo);
    ~UDSServer();
    virtual void run();
    void shutdown();
    void add(UDSSession *session);
    void remove(UDSSession *session);
private:
    int listenFd;
    LocalRepo *repo;
    std::set<UDSSession *> sessions;
    Mutex sessionLock;
};

class UDSSession : public Thread
{
public:
    UDSSession(UDSServer *uds, int fd, LocalRepo *repo);
    ~UDSSession();

    virtual void run();
    void forceExit();
    void serve();
    
    void printError(const std::string &what);
    void cmd_hello();
    void cmd_listObjs();
    void cmd_listCommits();
    void cmd_readObjs();
    void cmd_getObjInfo();
    void cmd_getHead();
    void cmd_getFSID();
private:
    UDSServer *uds;
    int fd;
    LocalRepo *repo;
};

#endif
