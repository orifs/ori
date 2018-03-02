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

#include <string>
#include <sstream>
#include <iostream>
#include <memory>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/orifile.h>
#include <ori/repo.h>
#include <ori/localrepo.h>
#include <ori/httpclient.h>
#include <ori/httprepo.h>
#include <ori/sshclient.h>
#include <ori/sshrepo.h>
#include <ori/peer.h>

using namespace std;

/********************************************************************
 *
 *
 * Peer
 *
 *
 ********************************************************************/

Peer::Peer()
    : instaCloning(false), url(""), repoId(""), peerFile("")
{
    instaCloning = false;
    url = "";
    repoId = "";
    peerFile = "";
}

Peer::Peer(const std::string &path)
{
    instaCloning = false;
    url = "";
    repoId = "";
    peerFile = path;

    if (OriFile_Exists(path)) {
        string blob = OriFile_ReadFile(path);
        fromBlob(blob);
    }
}

Peer::~Peer()
{
}

string
Peer::getBlob() const
{
    stringstream ss;
    string blob;
    
    blob = "url " + url + "\n";
    blob += "repoId " + repoId + "\n";
    if (instaCloning)
        blob += "instaCloning\n";

    return blob;
}

void
Peer::fromBlob(const string &blob)
{
    string line;
    stringstream ss(blob);

    while (getline(ss, line, '\n')) {
        if (line.substr(0, 4) == "url ") {
            url = line.substr(4);
        } else if (line.substr(0, 7) == "repoId ") {
            repoId = line.substr(7);
        } else if (line.substr(0, 12) == "instaCloning") {
            instaCloning = true;
        } else{
            printf("Unsupported peer feature!\n");
            PANIC();
        }
    }
}

void
Peer::setUrl(const std::string &path)
{
    // XXX: Verify URL or Path

    url = path;

    save();
}

std::string
Peer::getUrl() const
{
    return url;
}

void
Peer::setRepoId(const std::string &uuid)
{
    repoId = uuid;

    save();
}

std::string
Peer::getRepoId() const
{
    return repoId;
}

void
Peer::setInstaClone(bool ic)
{
    instaCloning = ic;

    save();
}

bool
Peer::isInstaCloning() const
{
    return instaCloning;
}

Repo *
Peer::getRepo()
{
    if (cachedRepo) {
        return cachedRepo.get();
    }

    if (Util_IsPathRemote(url)) {
        if (strncmp(url.c_str(), "http://", 7) == 0) {
            hc.reset(new HttpClient(url));
            cachedRepo.reset(new HttpRepo(hc.get()));
            hc->connect();
        } else {
            sc.reset(new SshClient(url));
            cachedRepo.reset(new SshRepo(sc.get()));
            sc->connect();
        }
    } else {
        char *path = realpath(url.c_str(), NULL);
        LocalRepo *local = new LocalRepo(path);
        local->open(path);
        cachedRepo.reset(local);
        free(path);
    }

    if (repoId == "") {
        setRepoId(cachedRepo->getUUID());
    }

    return cachedRepo.get();
}

void
Peer::save() const
{
    if (peerFile != "") {
        string blob = getBlob();

        OriFile_WriteFile(blob.data(), blob.size(), peerFile);
    }
}

