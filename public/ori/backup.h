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

#ifndef __BACKUP_H__
#define __BACKUP_H__

#include <stdexcept>
#include <boost/tr1/memory.hpp>

#include "object.h"

class BackupError : std::exception {
public:
    explicit BackupError(const std::string &msg)
        : msg(msg) {}
    ~BackupError() throw() {}
    const char *what() const throw() { return msg.c_str(); }
private:
    std::string msg;
};

class MemoryObject : public Object {
public:
    MemoryObject(const ObjectInfo &info, const std::string &payload)
        : Object(info), payload(payload)
    {
    }

    bytestream *getPayloadStream() {
        return new strstream(payload);
    }

    std::string getPayload() {
        return payload;
    }

private:
    std::string payload;
};

class BackupService {
public:
    typedef std::tr1::shared_ptr<BackupService> sp;

    // Is cached for the lifetime of a BackupService
    bool hasKey(const std::string &key) {
        HasKeyCache::iterator it = hasKeyCache.find(key);
        if (it != hasKeyCache.end()) {
            return (*it).second;
        }

        bool hk = realHasKey(key);
        hasKeyCache[key] = hk;
        return hk;
    }

    virtual bool realHasKey(const std::string &key) = 0;

    virtual bool getData(const std::string &key, std::string &out) = 0;
    virtual bool putData(const std::string &key, const std::string &data) = 0;
    virtual bool putFile(const std::string &key, const std::string &filename) = 0;

    virtual Object::sp getObj(const std::string &key) {
        std::string data;
        bool success = getData(key, data);
        if (!success) return Object::sp();

        ObjectInfo info;
        info.fromString(data.substr(0, ObjectInfo::SIZE));
        return Object::sp(new MemoryObject(info,
                    data.substr(ObjectInfo::SIZE)));
    }

    virtual bool putObj(const std::string &key, Object::sp obj) {
        std::string data = obj->getInfo().toString() + obj->getPayload();
        return putData(key, data);
    }

protected:
    typedef std::tr1::unordered_map<std::string, bool> HasKeyCache;
    HasKeyCache hasKeyCache;
};

#endif /* __BACKUP_H__ */

