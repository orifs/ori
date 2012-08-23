#ifndef __BACKUP_H__
#define __BACKUP_H__

#include <stdexcept>
#include <tr1/memory>

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
    virtual Object::sp getObj(const std::string &key) = 0;
    virtual bool putObj(const std::string &key, Object::sp obj) = 0;

protected:
    typedef std::tr1::unordered_map<std::string, bool> HasKeyCache;
    HasKeyCache hasKeyCache;
};

struct S3BucketContext;
class S3BackupService : public BackupService {
public:
    S3BackupService(const std::string &accessKeyID,
            const std::string &secretAccessKey,
            const std::string &bucketName);
    ~S3BackupService();

    bool realHasKey(const std::string &key);
    Object::sp getObj(const std::string &key);
    bool putObj(const std::string &key, Object::sp obj);

private:
    std::string accessKeyID;
    std::string secretAccessKey;
    std::string bucketName;

    std::tr1::shared_ptr<S3BucketContext> ctx;
};

#endif
