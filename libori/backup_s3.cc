#include "libs3.h"

#include "backup.h"

void _setStatusCB(S3Status status,
        const S3ErrorDetails *details,
        void *outp)
{
    S3Status *out = (S3Status *)outp;
    *out = status;
}

S3BackupService::S3BackupService(
        const std::string &accessKeyID,
        const std::string &secretAccessKey,
        const std::string &bucketName
        )
    : accessKeyID(accessKeyID),
      secretAccessKey(secretAccessKey),
      bucketName(bucketName)
{
    S3_initialize(NULL, S3_INIT_ALL, NULL);

    S3ResponseHandler handler;
    handler.propertiesCallback = NULL;
    handler.completeCallback = _setStatusCB;

    S3Status status;
    S3_test_bucket(S3ProtocolHTTPS,
            S3UriStyleVirtualHost,
            accessKeyID.c_str(),
            secretAccessKey.c_str(),
            NULL,
            bucketName.c_str(),
            0, NULL,
            NULL,
            &handler, &status);

    if (status != S3StatusOK) {
        throw BackupError(S3_get_status_name(status));
    }
}

S3BackupService::~S3BackupService()
{
    S3_deinitialize();
}

bool S3BackupService::realHasKey(const std::string &key)
{
    return false;
}

Object::sp S3BackupService::getObj(const std::string &key)
{
    return Object::sp();
}

bool S3BackupService::putObj(const std::string &key, Object::sp obj)
{
    return false;
}

