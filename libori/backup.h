#ifndef __BACKUP_H__
#define __BACKUP_H__

#include "object.h"

class MemoryObject : public Object {
public:
    MemoryObject();
};

class BackupService {
public:
    // Can be cached for the lifetime of a BackupService
    virtual bool hasObj(const std::string &key) = 0;
    virtual Object::sp getObj(const std::string &key) = 0;
    virtual bool putObj(const std::string &key, Object::sp obj) = 0;
};

class S3BackupService : public BackupService {
public:
    S3BackupService(const std::string &accessKeyID,
            const std::string &secretAccessKey);

    bool hasObj(const std::string &key);

private:
    std::string accessKeyID;
    std::string secretAccessKey;
};

#endif
