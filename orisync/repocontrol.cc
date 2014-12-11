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
        WARNING("%s", e.what());
    }

    try {
      localRepo = new LocalRepo();
      localRepo->open(path);
    } catch (exception e) {
        if (localRepo)
            delete localRepo;
        localRepo = NULL;
        WARNING("%s", e.what());
        throw SystemException();
    }


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
    if (udsRepo) {
        try {
            return udsRepo->getHead().hex();
        } catch (SystemException e) {
            WARNING("%s", e.what());
            return "";
        }
    }
    return localRepo->getHead().hex();
}

bool
RepoControl::hasCommit(const string &objId)
{
    // Caller needs to catch exceptions
    ObjectHash hash = ObjectHash::fromHex(objId);
    if (udsRepo)
        return udsRepo->hasObject(hash);
    return localRepo->hasObject(hash);
}

int
RepoControl::checkout(ObjectHash newHead)
{
    strwstream checkoutReq;
    checkoutReq.writePStr("checkout");
    checkoutReq.writeHash(newHead);
    checkoutReq.writeUInt8(/* force */0);
    try {
      strstream checkoutResp = udsRepo->callExt("FUSE", checkoutReq.str());
      if (checkoutResp.ended()) {
        WARNING("Unknown failure trying to checkout %s",
                newHead.hex().c_str());
        return -1;
      }
      if (checkoutResp.readUInt8() == 0) {
        string error;
        checkoutResp.readPStr(error);
        WARNING("Failed to checkout %s", error.c_str());
        return -1;
      }
    } catch (SystemException e) {
        WARNING("%s", e.what());
        return -1;
    }

    return 0;
}

string
RepoControl::pull(const string &host, const string &path)
{
    if (udsRepo) {
        // Do a snapshot first
        snapshot();

        strwstream pullReq;
        ObjectHash newHead;

        // Pull
        pullReq.writePStr("pull");
        pullReq.writePStr(host + (host.size() ? ":" : "") + path);

        try {
          strstream pullResp = udsRepo->callExt("FUSE", pullReq.str());
          if (pullResp.ended()) {
            WARNING("Unknown failure trying to pull from %s:%s",
                    host.c_str(), path.c_str());
            return "";
          }
          if (pullResp.readUInt8() == 0) {
            string error;
            pullResp.readPStr(error);
            WARNING("Failed to pull from %s:%s: %s",
                    host.c_str(), path.c_str(), error.c_str());
            return "";
          }
          pullResp.readHash(newHead);
        } catch (SystemException e) {
          WARNING("%s", e.what());
          return "";
        }
        // Checkout
        if (checkout(newHead) == -1)
            return "";

        LOG("RepoControl::pull: Update from %s:%s suceeded",
            host.c_str(), path.c_str());

        return "";
    }

    // Handle unmounted repository
    RemoteRepo::sp srcRepo(new RemoteRepo());
    if (!srcRepo->connect(host + (host.size() ? ":" : "") + path)) {
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

int
RepoControl::snapshot()
{
    if (udsRepo) {
      strwstream req;
      string msg = "Orisync automatic snapshot";

      req.writePStr("snapshot");
      req.writeUInt8(1); // hasMsg = true
      req.writeUInt8(0); // hasName = false
      
      req.writeLPStr(msg);

      try {
        strstream resp = udsRepo->callExt("FUSE", req.str());
        if (resp.ended()) {
        WARNING("RepoControl::snapshot: Snapshot failed with an unknown error!");
        return -1;
        }

        switch (resp.readUInt8())
        {
          case 0:
            //LOG("RepoControl::snapshot: No changes");
            return 0;
          case 1:
          {
            ObjectHash hash;
            resp.readHash(hash);
            return 1;
          }
          default:
            NOT_IMPLEMENTED(false);
        }
      } catch (SystemException e) {
        WARNING("%s", e.what());
        return -1;
      }
    }
    return -1;
}

/*
int
RepoControl::checkoutPrev(time_t time)
{
    map<time_t, ObjectHash>::iterator closeSS;
    closeSS = snapshots.lower_bound(time);
    if (closeSS == snapshots.end()) {
        LOG("Repocontrol::checkoutPrev: No snapshot found equal or larger than the given time");
        return -1;
    }
    return checkout(closeSS->second);
}
*/

void
RepoControl::gc(time_t time)
{
  if (udsRepo) {
        // Purge snapshot
        strwstream req;
        req.writePStr("purgesnapshot");
        req.writeUInt8(1); // time based
        req.writeInt64((int64_t)time);

        try {
          strstream resp = udsRepo->callExt("FUSE", req.str());
          if (resp.ended()) {
	          LOG("Repocontrol::gc: Garbage collection failed with an unknown error!");
	          return;
          }

          switch (resp.readUInt8())
          {
	          case 0:
	          {
	              DLOG("Repocontrol::gc: Garbage collection succeeded");
                break;
	          }
	          case 1:
	          {
	              string msg;
	              resp.readPStr(msg);
	              DLOG("%s", msg.c_str());
                break;
	          }
	          default:
    	          NOT_IMPLEMENTED(false);
          }
        } catch (SystemException e) {
          WARNING("%s", e.what());
          return;
        }
  }
}

bool
RepoControl::isMounted()
{
    if (udsRepo)
        return true;
    return false;
}
