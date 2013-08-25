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

#include <cstring>
#include <stdint.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h> // Needed for OriPriv
#include <errno.h>

#include <sys/un.h>

#include <string>
#include <map>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/systemexception.h>
#include <ori/repostore.h>
#include <ori/localrepo.h>
#include <ori/udsserver.h>

using namespace std;

UDSServer::UDSServer(LocalRepo *repo)
    : listenFd(-1), repo(repo)
{
    int status, sock, len;
    string fuseSock;
    struct sockaddr_un addr;

    // XXX: Create method to return fuse socket path
    fuseSock = repo->getUDSPath();

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
        throw SystemException();

    // XXX: Check that path doesn't exceed local.sun_path

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, fuseSock.c_str());
    unlink(fuseSock.c_str());
    len = fuseSock.size() + sizeof(addr.sun_family);
    status = bind(sock, (struct sockaddr *)&addr, len);
    if (status < 0)
        throw SystemException();

    status = listen(sock, 5);
    if (status < 0)
        throw SystemException();

    listenFd = sock;
}

UDSServer::~UDSServer()
{
    close(listenFd);
}

void
UDSServer::run()
{
    int client;
    socklen_t len;
    struct sockaddr_un remote;
    UDSSession *session;

    while (1) {
        len = sizeof(sockaddr_un);
        client = accept(listenFd, (struct sockaddr *)&remote, &len);
        if (client < 0) {
            perror("accept: ");
            continue;
        }

        LOG("UDSSession accepted!");
        session = new UDSSession(client, repo);
        session->serve();
        delete session;
        // Add session and call
    }
}

UDSSession::UDSSession(int fd, LocalRepo *repo)
    : fd(fd), repo(repo)
{
}

UDSSession::~UDSSession()
{
    close(fd);
}

#define OK 0
#define ERROR 1

void
UDSSession::printError(const std::string &what)
{
    fdwstream fs(fd);

    fs.writeUInt8(ERROR);
    fs.writePStr(what);
}

void UDSSession::serve() {
    fdstream fs(fd, -1);

    LocalRepoLock::sp lock(repo->lock());
    if (!lock.get()) {
        printError("Couldn't lock repo");
        exit(1);
    }

    uint8_t respOK = OK;
    write(fd, &respOK, 1);

    while (true) {
        // Get command
        std::string command;
        if (fs.readPStr(command) == 0)
            break;

        if (command == "hello") {
            cmd_hello();
        }
        else if (command == "list objs") {
            cmd_listObjs();
        }
        else if (command == "list commits") {
            cmd_listCommits();
        }
        else if (command == "readobjs") {
            cmd_readObjs();
        }
        else if (command == "getobjinfo") {
            cmd_getObjInfo();
        }
        else if (command == "get head") {
            cmd_getHead();
        }
        else if (command == "get fsid") {
            cmd_getFSID();
        }
        else {
            printError("Unknown command");
        }
    }
}

void UDSSession::cmd_hello()
{
    DLOG("hello");

    fdwstream fs(fd);
    fs.writeUInt8(OK);
    fs.writePStr(ORI_UDS_PROTO_VERSION);
}

void UDSSession::cmd_listObjs()
{
    DLOG("listObjs");
    fdwstream fs(fd);

    fs.writeUInt8(OK);

    std::set<ObjectInfo> objects = repo->listObjects();
    fs.writeUInt64(objects.size());
    for (std::set<ObjectInfo>::iterator it = objects.begin();
            it != objects.end();
            it++) {
        fs.writeInfo(*it);
    }
}

void UDSSession::cmd_listCommits()
{
    DLOG("listCommits");
    fdwstream fs(fd);

    fs.writeUInt8(OK);

    const std::vector<Commit> &commits = repo->listCommits();
    fs.writeUInt32(commits.size());
    for (size_t i = 0; i < commits.size(); i++) {
        std::string blob = commits[i].getBlob();
        fs.writePStr(blob);
    }
}

void UDSSession::cmd_readObjs()
{
    // Read object ids
    fdstream in(fd, -1);
    uint32_t numObjs = in.readUInt32();
    DLOG("readObjs: Transmitting %u objects", numObjs);

    std::vector<ObjectHash> objs;
    for (uint32_t i = 0; i < numObjs; i++) {
        ObjectHash hash;
        in.readHash(hash);
        objs.push_back(hash);
        DLOG("readObjs: %d of %d - %s", i + 1, numObjs, hash.hex().c_str());
    }

    fdwstream fs(fd);
    fs.writeUInt8(OK);
    repo->transmit(&fs, objs);
}

void UDSSession::cmd_getObjInfo()
{
    fdstream in(fd, -1);
    ObjectHash hash;
    ObjectInfo info;

    in.readHash(hash);

    fdwstream fs(fd);
    info = repo->getObjectInfo(hash);
    if (info.type == ObjectInfo::Null) {
        fs.writeUInt8(ERROR);
        return;
    }
    fs.writeUInt8(OK);
    fs.writeInfo(info);
}

void UDSSession::cmd_getHead()
{
    DLOG("getHead");
    fdwstream fs(fd);

    fs.writeUInt8(OK);
    fs.writeHash(repo->getHead());
}

void UDSSession::cmd_getFSID()
{
    DLOG("getFSID");
    fdwstream fs(fd);

    fs.writeUInt8(OK);
    fs.writePStr(repo->getUUID());
}

