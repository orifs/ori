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

#include "oricmd.h"
#include "oripriv.h"

using namespace std;

extern OriPriv *priv;
static UDSServer *server;

string
UDSExtensionCB(LocalRepo *repo, const string &data)
{
}

void
UDSServerStart(LocalRepo *repo)
{
    LOG("Starting Unix domain socket server");

    server = new UDSServer(repo);
    server->registerExt("FUSE", UDSExtensionCB);
    server->start();

    return;
}

void
UDSServerStop()
{
    server->shutdown();
    delete server;
}

