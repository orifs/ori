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

int
OriCommand::readSize()
{
    return resultBuffer.size();
}

int
OriCommand::read(char *buf, size_t size, size_t offset)
{
    size_t newSize;

    ASSERT(offset == 0);

    if (resultBuffer.size() == 0)
        return 0;
    if (size > resultBuffer.size())
        size = resultBuffer.size();

    memcpy(buf, resultBuffer.data(), size);
    newSize = resultBuffer.size() - size;
    memmove(&resultBuffer[0], &resultBuffer[size], newSize);
    resultBuffer.resize(newSize);

    return size;
}

/*
 * Main control entry point that dispatches to the various commands.
 */
int
OriCommand::write(const char *buf, size_t size, size_t offset)
{
    int status = -EIO;
    int argc = 0;
    char *argv[10];
    string cmd;

    // XXX: Support arguments
    cmd.assign(buf, size);

    if (strncmp(buf, "fsck", size) == 0)
        status = cmd_fsck(argc, (const char **)argv);
    if (strncmp(buf, "log", size) == 0)
        status = cmd_log(argc, (const char **)argv);
    if (strncmp(buf, "show", size) == 0)
        status = cmd_show(argc, (const char **)argv);
    if (strncmp(buf, "snapshot", size) == 0)
        status = cmd_snapshot(argc, (const char **)argv);
    if (strncmp(buf, "snapshots", size) == 0)
        status = cmd_snapshots(argc, (const char **)argv);
    if (strncmp(buf, "status", size) == 0)
        status = cmd_status(argc, (const char **)argv);
    if (strncmp(buf, "tip", size) == 0)
        status = cmd_tip(argc, (const char **)argv);

    // Need to return bytes written on success
    if (status == 0)
        status = size;

    return status;
}

void
OriCommand::printf(const char *fmt, ...)
{
    char buffer[OUTPUT_MAX];
    va_list args;

    va_start(args, fmt);

    vsnprintf(buffer, OUTPUT_MAX, fmt, args);

    resultBuffer += buffer;

    va_end(args);
}

int
OriCommand::cmd_fsck(int argc, const char *argv[])
{
    FUSE_LOG("Command: fsck");

    priv->fsck(true);

    return 0;
}

int
OriCommand::cmd_log(int argc, const char *argv[])
{
#if defined(DEBUG) || defined(ORI_PERF)
    Stopwatch sw = Stopwatch();
    sw.start();
#endif /* DEBUG */

    FUSE_PLOG("Command: log");

    ObjectHash commit = priv->getTip();

    while (!commit.isEmpty()) {
        Commit c = priv->repo->getCommit(commit);
        pair<ObjectHash, ObjectHash> p = c.getParents();
        time_t timeVal = c.getTime();
        char timeStr[26];

        ctime_r(&timeVal, timeStr);

        printf("Commit:  %s\n", commit.hex().c_str());
        printf("Parents: %s %s\n",
               p.first.isEmpty() ? "" : p.first.hex().c_str(),
               p.second.isEmpty() ? "" : p.second.hex().c_str());
        printf("Tree:    %s\n", c.getTree().hex().c_str());
        printf("Author:  %s\n", c.getUser().c_str());
        printf("Date:    %s\n", timeStr);
        printf("%s\n\n", c.getMessage().c_str());

        commit = c.getParents().first;
        // XXX: Handle merge cases
    }
   
#if defined(DEBUG) || defined(ORI_PERF)
    sw.stop();
    FUSE_PLOG("log elapsed %ldus", sw.getElapsedTime());
#endif /* DEBUG */

    return 0;
}

int
OriCommand::cmd_show(int argc, const char *argv[])
{
    FUSE_LOG("Command: show");

    printf("--- Repository ---\n");
    printf("Root: %s\n", priv->repo->getRootPath().c_str());
    printf("UUID: %s\n", priv->repo->getUUID().c_str());
    printf("Version: %s\n", priv->repo->getVersion().c_str());
    printf("HEAD: %s\n", priv->getTip().hex().c_str());

    return 0;
}

int
OriCommand::cmd_snapshot(int argc, const char *argv[])
{
#if defined(DEBUG) || defined(ORI_PERF)
    Stopwatch sw = Stopwatch();
    sw.start();
#endif /* DEBUG */

    FUSE_PLOG("Command: snapshot");

    string msg = priv->commit("FUSE commit from user");

    printf("%s\n", msg.c_str());

#if defined(DEBUG) || defined(ORI_PERF)
    sw.stop();
    FUSE_PLOG("commit result: %s", msg.c_str());
    FUSE_PLOG("commit elapsed %ldus", sw.getElapsedTime());
#endif /* DEBUG */

    return 0;
}

int
OriCommand::cmd_snapshots(int argc, const char *argv[])
{
    FUSE_LOG("Command: snapshots");

    map<string, ObjectHash> snapshots = priv->repo->listSnapshots();
    map<string, ObjectHash>::iterator it;

    for (it = snapshots.begin(); it != snapshots.end(); it++)
    {
        printf("%s\n", (*it).first);
    }

    return 0;
}

int
OriCommand::cmd_status(int argc, const char *argv[])
{
#if defined(DEBUG) || defined(ORI_PERF)
    Stopwatch sw = Stopwatch();
    sw.start();
#endif /* DEBUG */

    FUSE_PLOG("Command: status");

    map<string, OriFileState::StateType> diff = priv->getDiff();
    map<string, OriFileState::StateType>::iterator it;

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

        printf("%c   %s\n", type, it->first.c_str());
    }

#if defined(DEBUG) || defined(ORI_PERF)
    sw.stop();
    FUSE_PLOG("status elapsed %ldus", sw.getElapsedTime());
#endif /* DEBUG */

    return 0;
}

int
OriCommand::cmd_tip(int argc, const char *argv[])
{
    FUSE_LOG("Command: tip");

    resultBuffer = priv->getTip().hex() + "\n";

    return 0;
}

