#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <fcntl.h>

#include "libs3.h"

#include "backup.h"

void _setStatusCB(S3Status status,
        const S3ErrorDetails *details,
        void *outp)
{
    S3Status *out = (S3Status *)outp;
    *out = status;
}

S3Status _ignoreRespPropsCB(const S3ResponseProperties *props,
        void *cbdata)
{
    return S3StatusOK;
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
#ifdef USE_FAKES3
    S3Protocol s3proto = S3ProtocolHTTP;
    S3UriStyle uriStyle = S3UriStylePath;
    const char *hostname = "localhost:4567";
    S3_initialize(NULL, S3_INIT_ALL, hostname);
    fprintf(stderr, "Using fakes3\n");
#else
    S3Protocol s3proto = S3ProtocolHTTP;
    S3UriStyle uriStyle = S3UriStyleVirtualHost;
    const char *hostname = NULL;
    S3_initialize(NULL, S3_INIT_ALL, NULL);
#endif


    S3ResponseHandler handler;
    handler.propertiesCallback = _ignoreRespPropsCB;
    handler.completeCallback = _setStatusCB;

#ifndef USE_FAKES3
    S3Status status;
    S3_test_bucket(s3proto,
            uriStyle,
            accessKeyID.c_str(),
            secretAccessKey.c_str(),
            hostname,
            bucketName.c_str(),
            0, NULL,
            NULL,
            &handler, &status);

    if (status != S3StatusOK) {
        throw BackupError(S3_get_status_name(status));
    }
#endif

    ctx.reset(new S3BucketContext());
    ctx->hostName = hostname;
    ctx->bucketName = bucketName.c_str();
    ctx->protocol = s3proto;
    ctx->uriStyle = uriStyle;
    ctx->accessKeyId = accessKeyID.c_str();
    ctx->secretAccessKey = secretAccessKey.c_str();
}

S3BackupService::~S3BackupService()
{
    S3_deinitialize();
}

#ifdef USE_FAKES3
void S3BackupService::setHostname(const std::string &hostname)
{
    _hostname = hostname;
    ctx->hostName = _hostname.c_str();
    printf("Set hostname to %s\n", hostname.c_str());
}
#endif

bool S3BackupService::realHasKey(const std::string &key)
{
    S3ResponseHandler handler;
    handler.propertiesCallback = _ignoreRespPropsCB;
    handler.completeCallback = _setStatusCB;

    S3Status status;
    S3_head_object(ctx.get(), key.c_str(), NULL,
            &handler, &status);
    if (status != S3StatusOK) {
        if (status != S3StatusHttpErrorNotFound)
            fprintf(stderr, "realHasKey: %s\n", S3_get_status_name(status));
        return false;
    }
    
    return true;
}

////////////////////////////////
// getData

template <typename T>
void _setStructStatusCB(S3Status status,
        const S3ErrorDetails *details,
        void *outp)
{
    T *struct_p = (T *)outp;
    struct_p->status = status;
}

struct _getDataData {
    _getDataData(std::string &out) : out(out) {}
    S3Status status;
    std::string &out;
};

S3Status _getDataCB(int bufferSize,
        const char *buffer,
        void *cbdata)
{
    _getDataData *od = (_getDataData *)cbdata;
    size_t oldSize = od->out.size();
    od->out.resize(oldSize + bufferSize);
    memcpy(&od->out[oldSize], buffer, bufferSize);

    return S3StatusOK;
}

bool S3BackupService::getData(const std::string &key, std::string &out)
{
    S3GetObjectHandler handler;
    handler.responseHandler.propertiesCallback = _ignoreRespPropsCB;
    handler.responseHandler.completeCallback = _setStructStatusCB<_getDataData>;
    handler.getObjectDataCallback = _getDataCB;

    _getDataData data(out);
    out.resize(0);
    S3_get_object(ctx.get(), key.c_str(), NULL,
            0, 0, NULL, &handler, &data);

    if (data.status != S3StatusOK) {
        fprintf(stderr, "getData: %s\n", S3_get_status_name(data.status));
        return false;
    }

    return true;
}

////////////////////////////////
// putData

struct _putDataData {
    S3Status status;

    std::string payload;
    uint64_t offset;
};

int _putDataCB(int bufSize, char *buf, void *cbdata) {
    _putDataData *od = (_putDataData *)cbdata;

    size_t to_copy = MIN(bufSize, od->payload.size() - od->offset);

    memcpy(buf, &od->payload[od->offset], to_copy);
    od->offset += to_copy;

    return to_copy;
}

bool S3BackupService::putData(const std::string &key, const std::string &data)
{
    _putDataData od;
    od.payload = data;
    od.offset = 0;

    S3PutObjectHandler handler;
    handler.responseHandler.propertiesCallback = _ignoreRespPropsCB;
    handler.responseHandler.completeCallback = _setStructStatusCB<_putDataData>;
    handler.putObjectDataCallback = _putDataCB;

    S3_put_object(ctx.get(), key.c_str(),
            data.size(),
            NULL, NULL, &handler, &od);

    if (od.status != S3StatusOK) {
        fprintf(stderr, "putObj: %s\n", S3_get_status_name(od.status));
        return false;
    }

    return true;
}

////////////////////////////////
// putFile

struct _putFileData {
    _putFileData(int fd) : fd(fd) {}
    ~_putFileData() { if (fd > 0) close(fd); }

    S3Status status;
    int fd;
};

int _putFileCB(int bufSize, char *buf, void *cbdata) {
    _putFileData *pd = (_putFileData *)cbdata;
    return read(pd->fd, buf, bufSize);
}

bool S3BackupService::putFile(const std::string &key, const std::string &filename)
{
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("S3BackupService::putFile open");
        return false;
    }

    struct stat sb;
    if (stat(filename.c_str(), &sb) < 0) {
        perror("S3BackupService::putFile stat");
        return false;
    }

    _putFileData data(fd);

    S3PutObjectHandler handler;
    handler.responseHandler.propertiesCallback = _ignoreRespPropsCB;
    handler.responseHandler.completeCallback = _setStructStatusCB<_putFileData>;
    handler.putObjectDataCallback = _putFileCB;

    S3_put_object(ctx.get(), key.c_str(),
            sb.st_size,
            NULL, NULL, &handler, &data);

    if (data.status != S3StatusOK) {
        fprintf(stderr, "putFile: %s\n", S3_get_status_name(data.status));
        return false;
    }

    return true;
}

