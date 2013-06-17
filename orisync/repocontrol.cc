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

#include <ori/localrepo.h>

#include "repocontrol.h"

using namespace std;

RepoControl::RepoControl(const string &path)
    : path(path), uuid(""), repo(path)
{
}

RepoControl::~RepoControl()
{
}

void
RepoControl::open()
{
    repo.open();
    path = repo.getRootPath();
    uuid = repo.getUUID();
}

void
RepoControl::close()
{
    repo.close();
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
    return repo.getHead().hex();
}

string
RepoControl::push(const string &host, const string &token)
{
}

string
RepoControl::pull(const string &host, const string &token)
{
}

