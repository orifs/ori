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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>
#include <map>
#include <boost/tr1/memory.hpp>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/stopwatch.h>
#include <oriutil/rwlock.h>
#include <oriutil/systemexception.h>
#include <ori/commit.h>
#include <ori/localrepo.h>

#include "logging.h"
#include "oricmd.h"
#include "oripriv.h"

using namespace std;

#define OUTPUT_MAX      1024

OriCommand::OriCommand(OriPriv *priv)
{
    this->priv = priv;
}

OriCommand::~OriCommand()
{
}

/*
 * Main control entry point that dispatches to the various commands.
 */
string
OriCommand::process(const string &data)
{
    string cmd;
    strstream str = strstream(data);

    str.readPStr(cmd);

    if (cmd == "fsck")
        return cmd_fsck(str);
    if (cmd == "snapshot")
        return cmd_snapshot(str);
    if (cmd == "snapshots")
        return cmd_snapshots(str);
    if (cmd == "status")
        return cmd_status(str);
    if (cmd == "pull")
        return cmd_pull(str);

    // Makes debugging easier when a bad request comes in
    return "UNSUPPORTED REQUEST";
}

string
OriCommand::cmd_fsck(strstream &str)
{
    FUSE_LOG("Command: fsck");

    priv->fsck();

    return 0;
}

string
OriCommand::cmd_snapshot(strstream &str)
{
#if defined(DEBUG) || defined(ORI_PERF)
    Stopwatch sw = Stopwatch();
    sw.start();
#endif /* DEBUG */

    FUSE_PLOG("Command: snapshot");

    uint8_t hasMsg, hasName;
    string msg, name;
    Commit c;
    strwstream resp;

    // Parse Command
    hasMsg = str.readUInt8();
    hasName = str.readUInt8();
    if (hasMsg) {
        str.readLPStr(msg);
        c.setMessage(msg);
    }
    if (hasName) {
        str.readLPStr(name);
        c.setSnapshot(name);
    }

    RWKey::sp lock = priv->nsLock.writeLock();
    ObjectHash hash = priv->commit(c);
    lock.reset();
    if (hash.isEmpty()) {
        resp.writeUInt8(0);
    } else {
        resp.writeUInt8(1);
        resp.writeHash(hash);
    }

#if defined(DEBUG) || defined(ORI_PERF)
    sw.stop();
    if (hash.isEmpty())
        FUSE_PLOG("snapshot not taken");
    FUSE_PLOG("snapshot result: %s", hash.hex().c_str());
    FUSE_PLOG("snapshot elapsed %ldus", sw.getElapsedTime());
#endif /* DEBUG */

    return resp.str();
}

string
OriCommand::cmd_snapshots(strstream &str)
{
    FUSE_LOG("Command: snapshots");

    map<string, ObjectHash> snapshots = priv->repo->listSnapshots();
    map<string, ObjectHash>::iterator it;
    strwstream resp;

    resp.writeUInt32(snapshots.size());
    for (it = snapshots.begin(); it != snapshots.end(); it++)
    {
        resp.writePStr((*it).first.c_str());
    }

    return resp.str();;
}

string
OriCommand::cmd_status(strstream &str)
{
#if defined(DEBUG) || defined(ORI_PERF)
    Stopwatch sw = Stopwatch();
    sw.start();
#endif /* DEBUG */

    FUSE_PLOG("Command: status");

    map<string, OriFileState::StateType> diff;
    map<string, OriFileState::StateType>::iterator it;
    strwstream resp;

    RWKey::sp lock = priv->nsLock.writeLock();
    diff = priv->getDiff();
    lock.reset();

    resp.writeUInt32(diff.size());
    for (it = diff.begin(); it != diff.end(); it++) {
        char type = '!';

        if (it->second == OriFileState::Created)
            type = 'A';
        else if (it->second == OriFileState::Modified)
            type = 'M';
        else if (it->second == OriFileState::Deleted)
            type = 'D';
        else
            ASSERT(false);

        resp.writeUInt8(type);
        resp.writeLPStr(it->first);
    }

#if defined(DEBUG) || defined(ORI_PERF)
    sw.stop();
    FUSE_PLOG("status elapsed %ldus", sw.getElapsedTime());
#endif /* DEBUG */

    return resp.str();
}

string
OriCommand::cmd_pull(strstream &str)
{
#if defined(DEBUG) || defined(ORI_PERF)
    Stopwatch sw = Stopwatch();
    sw.start();
#endif /* DEBUG */

    FUSE_PLOG("Command: pull");

    bool success;
    string srcPath;
    string error;
    ObjectHash hash;
    RemoteRepo srcRepo = RemoteRepo();
    strwstream resp;

    // Parse Command
    str.readPStr(srcPath);

    if (srcPath == "") {
        map<string, Peer> peers = priv->getRepo()->getPeers();
        map<string, Peer>::iterator it = peers.find("origin");

        if (it == peers.end()) {
            error = "No default repository to pull from.";
            goto error;
        }
        srcPath = (*it).second.getUrl();
    }

    try {
        success = srcRepo.connect(srcPath);
    } catch (exception &e) {
        error = e.what();
        goto error;
    }
    if (success) {
        hash = srcRepo->getHead();

        // XXX: Change to a repo lock
        RWKey::sp lock = priv->nsLock.writeLock();
        priv->getRepo()->pull(srcRepo.get());
        // XXX: Refcounts need to be done incrementally or rebuilt after
        lock.reset();
    } else {
        error = "Connection failed!";
    }

error:
    if (hash.isEmpty()) {
        resp.writeUInt8(0);
        resp.writePStr(error);
    } else {
        resp.writeUInt8(1);
        resp.writeHash(hash);
    }

#if defined(DEBUG) || defined(ORI_PERF)
    sw.stop();
    FUSE_PLOG("pull up to: %s", hash.hex().c_str());
    FUSE_PLOG("pull elapsed %ldus", sw.getElapsedTime());
#endif /* DEBUG */

    return resp.str();
}

