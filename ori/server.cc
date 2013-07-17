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
#include <fcntl.h>
#include <errno.h>

#include <string>
#include <map>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <ori/server.h>
#include <ori/localrepo.h>

extern LocalRepo repository;

SshServer::SshServer()
{
}

#define OK 0
#define ERROR 1

void printError(const std::string &what)
{
    fdwstream fs(STDOUT_FILENO);
    fs.writeUInt8(ERROR);
    fs.writePStr(what);
    fflush(stdout);
}

void SshServer::serve() {
    fdstream fs(STDIN_FILENO, -1);

    LocalRepoLock::sp lock(repository.lock());
    if (!lock.get()) {
        printError("Couldn't lock repo");
        exit(1);
    }

    uint8_t respOK = OK;
    write(STDOUT_FILENO, &respOK, 1);
    fsync(STDOUT_FILENO);
    fflush(stdout);


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
        else {
            printError("Unknown command");
        }

        fflush(stdout);
        fsync(STDOUT_FILENO);
    }

    fflush(stdout);
    fsync(STDOUT_FILENO);
}

void SshServer::cmd_hello()
{
    DLOG("hello");
    fdwstream fs(STDOUT_FILENO);
    fs.writeUInt8(OK);
    fs.writePStr(ORI_PROTO_VERSION);
}

void SshServer::cmd_listObjs()
{
    DLOG("listObjs");
    fdwstream fs(STDOUT_FILENO);
    fs.writeUInt8(OK);

    std::set<ObjectInfo> objects = repository.listObjects();
    fs.writeUInt64(objects.size());
    for (std::set<ObjectInfo>::iterator it = objects.begin();
            it != objects.end();
            it++) {
        fs.writeInfo(*it);
    }
}

void SshServer::cmd_listCommits()
{
    DLOG("listCommits");
    fdwstream fs(STDOUT_FILENO);
    fs.writeUInt8(OK);

    const std::vector<Commit> &commits = repository.listCommits();
    fs.writeUInt32(commits.size());
    for (size_t i = 0; i < commits.size(); i++) {
        std::string blob = commits[i].getBlob();
        fs.writePStr(blob);
    }
}

void SshServer::cmd_readObjs()
{
    // Read object ids
    fdstream in(STDIN_FILENO, -1);
    uint32_t numObjs = in.readUInt32();
    DLOG("readObjs: Transmitting %u objects", numObjs);
    std::vector<ObjectHash> objs;
    for (uint32_t i = 0; i < numObjs; i++) {
        ObjectHash hash;
        in.readHash(hash);
        objs.push_back(hash);
        DLOG("readObjs: %d of %d - %s", i + 1, numObjs, hash.hex().c_str());
    }

    fdwstream fs(STDOUT_FILENO);
    fs.writeUInt8(OK);
    repository.transmit(&fs, objs);
}

void SshServer::cmd_getObjInfo()
{
    fdstream in(STDIN_FILENO, -1);
    ObjectHash hash;
    ObjectInfo info;

    in.readHash(hash);
    fdwstream fs(STDOUT_FILENO);
    info = repository.getObjectInfo(hash);
    if (info.type == ObjectInfo::Null) {
        fs.writeUInt8(ERROR);
        return;
    }
    fs.writeUInt8(OK);
    fs.writeInfo(info);
}

void SshServer::cmd_getHead()
{
    DLOG("getHead");
    fdwstream fs(STDOUT_FILENO);
    fs.writeUInt8(OK);
    fs.writeHash(repository.getHead());
}



void ae_flush() {
    fflush(stdout);
    fsync(STDOUT_FILENO);
}

int cmd_sshserver(int argc, const char *argv[])
{
    atexit(ae_flush);

    Util_SetBlocking(STDIN_FILENO, true);
    // TODO: necessary?
    Util_SetBlocking(STDOUT_FILENO, true);

    // Disable output buffering
    setvbuf(stdout, NULL, _IONBF, 0); // libc
#ifdef __APPLE__
    fcntl(STDOUT_FILENO, F_NOCACHE, 1); // os x
#endif /* __APPLE__ */

    if (argc < 2) {
        printError("Need repository name");
        exit(1);
    }
    try {
        repository.open(argv[1]);
    } catch (std::exception &e) {
        printError(e.what());
        exit(101);
    }

    if (ori_open_log(repository.getLogPath()) < 0) {
        printError("Couldn't open log");
        exit(1);
    }

    LOG("Starting SSH server");

    SshServer server;
    server.serve();
    return 0;
}
