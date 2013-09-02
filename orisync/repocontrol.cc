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
    : path(path), uuid(""), udsClient(0), udsRepo(0), localRepo(0)
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
        udsRepo = new UDSRepo(udsClient);
    } catch (SystemException e) {
        if (udsRepo)
            delete udsRepo;
        udsRepo = NULL;
    }

    localRepo = new LocalRepo();
    localRepo->open(path);

    if (udsRepo)
        uuid = udsRepo->getUUID();
    else
        uuid = localRepo->getUUID();
}

void
RepoControl::close()
{
    if (udsClient) {
        delete udsRepo;
        delete udsClient;
    }
    if (localRepo) {
        localRepo->close();
        delete localRepo;
    }

    udsClient = NULL;
    udsRepo = NULL;
    localRepo = NULL;
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
    if (udsRepo)
        return udsRepo->getHead().hex();
    return localRepo->getHead().hex();
}

bool
RepoControl::hasCommit(const string &objId)
{
    ObjectHash hash = ObjectHash::fromHex(objId);
    if (udsRepo)
        return udsRepo->hasObject(hash);
    return localRepo->hasObject(hash);
}

string
RepoControl::pull(const string &host, const string &path)
{
    if (udsRepo) {
        strwstream pullReq;
        ObjectHash newHead;

        // Pull
        pullReq.writePStr("pull");
        pullReq.writePStr(host + ":" + path);

        strstream pullResp = udsRepo->callExt("FUSE", pullReq.str());
        if (pullResp.ended()) {
            WARNING("Unknown failure trying to pull from %s:%s",
                    host.c_str(), path.c_str());
            return "";
        }
        if (pullResp.readUInt8() == 1) {
            string error;
            pullResp.readPStr(error);
            WARNING("Failed to pull from %s:%s: %s",
                    host.c_str(), path.c_str(), error.c_str());
            return "";
        }
        pullResp.readHash(newHead);

        // Checkout
        strwstream checkoutReq;
        checkoutReq.writePStr("checkout");
        checkoutReq.writeHash(newHead);
        checkoutReq.writeUInt8(/* force */0);
        strstream checkoutResp = udsRepo->callExt("FUSE", checkoutReq.str());
        if (checkoutResp.ended()) {
            WARNING("Unknown failure trying to checkout %s",
                    newHead.hex().c_str());
            return "";
        }
        if (checkoutResp.readUInt8() == 0) {
            string error;
            checkoutResp.readPStr(error);
            WARNING("Failed to checkout %s", error.c_str());
            return "";
        }

        LOG("RepoControl::pull: Update from %s:%s suceeded",
            host.c_str(), path.c_str());

        return "";
    }

    // Handle unmounted repository
    RemoteRepo::sp srcRepo(new RemoteRepo());
    if (!srcRepo->connect(host + ":" + path)) {
        LOG("RepoControl::pull: Failed to connect to source %s", host.c_str());
        return  "";
    }

    ObjectHash newHead = srcRepo->get()->getHead();
    localRepo->pull(srcRepo->get());
    localRepo->updateHead(newHead);

    LOG("RepoControl::pull: Update succeeded");

    return newHead.hex();
}

string
RepoControl::push(const string &host, const string &path)
{
    NOT_IMPLEMENTED(false);

    return "";
}

