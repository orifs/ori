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

#ifndef __UDSREPO_H__
#define __UDSREPO_H__

#include <string>
#include <vector>
#include <deque>

#include "repo.h"
#include "udsclient.h"

class UDSObject;
class UDSRepo : public Repo
{
public:
    friend class UDSObject;
    UDSRepo(UDSClient *client);
    ~UDSRepo();

    std::string getUUID();
    ObjectHash getHead();
    int distance();

    Object::sp getObject(const ObjectHash &id);
    ObjectInfo getObjectInfo(const ObjectHash &id);
    bool hasObject(const ObjectHash &id);
    bytestream *getObjects(const ObjectHashVec &objs);
    std::set<ObjectInfo> listObjects();
    int addObject(ObjectType type, const ObjectHash &hash,
            const std::string &payload);
    std::vector<Commit> listCommits();

    // Transport
    virtual void transmit(bytewstream *out, const ObjectHashVec &objs);
private:
    UDSClient *client;
    
    std::string &_payload(const ObjectHash &id);
    void _addPayload(const ObjectHash &id, const std::string &payload);
    void _clearPayload(const ObjectHash &id);

    std::map<ObjectHash, std::string> payloads;

    std::tr1::unordered_set<ObjectHash> *containedObjs;
};

class UDSObject : public Object
{
public:
    UDSObject(UDSRepo *repo, ObjectInfo info);
    ~UDSObject();

    bytestream *getPayloadStream();

private:
    UDSRepo *repo;
};

#endif /* __UDSREPO_H__ */
