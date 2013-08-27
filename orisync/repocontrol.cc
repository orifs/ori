/*
 * Copyright (c) 2013 Stanford University
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

#include <string>

#include <oriutil/debug.h>
#include <oriutil/systemexception.h>
#include <ori/repo.h>
#include <ori/localrepo.h>
#include <ori/udsclient.h>
#include <ori/udsrepo.h>

#include "repocontrol.h"

using namespace std;

RepoControl::RepoControl(const string &path)
    : path(path), uuid(""), udsClient(0), repo(0)
{
}

RepoControl::~RepoControl()
{
}

void
RepoControl::open()
{
    try {
        udsClient = new UDSClient(path);
        udsClient->connect();
        repo = new UDSRepo(udsClient);
    } catch (SystemException e) {
        if (repo)
            delete repo;
    }

    LocalRepo *lrepo = new LocalRepo();
    lrepo->open(path);
    repo = lrepo;

    uuid = repo->getUUID();
}

void
RepoControl::close()
{
    if (udsClient) {
        delete udsClient;
    } else if (repo) { // LocalRepo
        LocalRepo *lrepo = (LocalRepo *)repo;
        lrepo->close();
    }

    if (repo)
        delete repo;

    repo = NULL;
    udsClient = NULL;
}

string
RepoControl::getPath()
{
    return path;
}

string
RepoControl::getUUID()
{
    ASSERT(uuid != "");
    return uuid;
}

string
RepoControl::getHead()
{
    return repo->getHead().hex();
}

bool
RepoControl::hasCommit(const string &objId)
{
    ObjectHash hash = ObjectHash::fromHex(objId);
    return repo->hasObject(hash);
}

string
RepoControl::push(const string &host, const string &token)
{
}

string
RepoControl::pull(const string &host, const string &token)
{
}

