#ifndef __S3BACKUP_H__
#define __S3BACKUP_H__

#include <stdexcept>
#include <tr1/memory>

#include <ori/object.h>
#include <ori/backup.h>

struct S3BucketContext;
class S3BackupService : public BackupService {
public:
    S3BackupService(const std::string &accessKeyID,
            const std::string &secretAccessKey,
            const std::string &bucketName);
    ~S3BackupService();

#ifdef USE_FAKES3
    void setHostname(const std::string &hostname);
#endif

    bool realHasKey(const std::string &key);
    bool getData(const std::string &key, std::string &out);
    bool putFile(const std::string &key, const std::string &filename);
    bool putData(const std::string &key, const std::string &data);

private:
    std::string accessKeyID;
    std::string secretAccessKey;
    std::string bucketName;
    std::string _hostname;

    std::tr1::shared_ptr<S3BucketContext> ctx;
};

#endif /* __S3BACKUP_H__ */

