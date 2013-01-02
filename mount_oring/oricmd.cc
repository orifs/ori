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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>
#include <map>

#include <ori/debug.h>
#include <ori/oriutil.h>
#include <ori/posixexception.h>
#include <ori/rwlock.h>
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

int
OriCommand::write(const char *buf, size_t size, size_t offset)
{
    int argc = 0;
    char argv[10];
    string cmd;

    cmd.assign(buf, size);

    printf("%s\n", buf);

    if (strncmp(buf, "fsck", size) == 0)
        return cmd_fsck(argc, (const char **)&argv);
    if (strncmp(buf, "tip", size) == 0)
        return cmd_tip(argc, (const char **)&argv);

    return -EIO;
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
    priv->fsck(true);

    return 0;
}

int
OriCommand::cmd_tip(int argc, const char *argv[])
{
    resultBuffer = priv->getTip().hex() + "\n";

    return 0;
}

