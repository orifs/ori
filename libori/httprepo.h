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

#ifndef __HTTPREPO_H__
#define __HTTPREPO_H__

#include <string>
#include <vector>

#include "repo.h"

class HttpObject;
class HttpRepo : public Repo
{
public:
    friend class HttpObject;
    HttpRepo(HttpClient *client);
    ~HttpRepo();

    void preload(const std::vector<std::string> &objs);

    std::string getHead();

    Object *getObject(const std::string &id);
    bool hasObject(const std::string &id);
    std::set<ObjectInfo> listObjects();
    int addObjectRaw(const ObjectInfo &info, bytestream *bs);
    std::vector<Commit> listCommits();

private:
    HttpClient *client;
    
    std::string &_payload(const std::string &id);
    void _addPayload(const std::string &id, const std::string &payload);
    void _clearPayload(const std::string &id);

    std::map<std::string, std::string> payloads;
};

class HttpObject : public Object
{
public:
    HttpObject(HttpRepo *repo, ObjectInfo info);
    ~HttpObject();

    bytestream::ap getPayloadStream();
    bytestream::ap getStoredPayloadStream();
    size_t getStoredPayloadSize();

private:
    HttpRepo *repo;
};

#endif /* __HTTPREPO_H__ */
