
#include <stdio.h>
#include <stdlib.h>

#include <sys/param.h>

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
    S3_initialize(NULL, S3_INIT_ALL, NULL);

    S3ResponseHandler handler;
    handler.propertiesCallback = _ignoreRespPropsCB;
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

    ctx.reset(new S3BucketContext());
    ctx->hostName = NULL;
    ctx->bucketName = bucketName.c_str();
    ctx->protocol = S3ProtocolHTTPS;
    ctx->uriStyle = S3UriStyleVirtualHost;
    ctx->accessKeyId = accessKeyID.c_str();
    ctx->secretAccessKey = secretAccessKey.c_str();
}

S3BackupService::~S3BackupService()
{
    S3_deinitialize();
}

bool S3BackupService::realHasKey(const std::string &key)
{
    S3ResponseHandler handler;
    handler.propertiesCallback = _ignoreRespPropsCB;
    handler.completeCallback = _setStatusCB;

    S3Status status;
    S3_head_object(ctx.get(), key.c_str(), NULL,
            &handler, &status);
    if (status != S3StatusOK) {
        fprintf(stderr, "%s\n", S3_get_status_name(status));
        return false;
    }
    
    return true;
}

////////////////////////////////
// getObj

template <typename T>
void _setStructStatusCB(S3Status status,
        const S3ErrorDetails *details,
        void *outp)
{
    T *struct_p = (T *)outp;
    struct_p->status = status;
}

struct _getObjData {
    S3Status status;
    std::string data;
};

S3Status _getObjDataCB(int bufferSize,
        const char *buffer,
        void *cbdata)
{
    _getObjData *od = (_getObjData *)cbdata;
    size_t oldSize = od->data.size();
    od->data.resize(oldSize + bufferSize);
    memcpy(&od->data[oldSize], buffer, bufferSize);

    return S3StatusOK;
}

Object::sp S3BackupService::getObj(const std::string &key)
{
    S3GetObjectHandler handler;
    handler.responseHandler.propertiesCallback = _ignoreRespPropsCB;
    handler.responseHandler.completeCallback = _setStructStatusCB<_getObjData>;
    handler.getObjectDataCallback = _getObjDataCB;

    _getObjData data;
    S3_get_object(ctx.get(), key.c_str(), NULL,
            0, 0, NULL, &handler, &data);

    if (data.status != S3StatusOK) {
        fprintf(stderr, "%s\n", S3_get_status_name(data.status));
        return Object::sp();
    }

    ObjectInfo info;
    info.fromString(data.data.substr(0, ObjectInfo::SIZE));

    return Object::sp(new MemoryObject(info, data.data.substr(ObjectInfo::SIZE)));
}

////////////////////////////////
// putObj

struct _putObjData {
    S3Status status;

    bool info_written;
    std::string info_str;
    std::string payload;
    uint64_t offset;
};

int _putObjCB(int bufSize, char *buf, void *cbdata) {
    _putObjData *od = (_putObjData *)cbdata;

    if (!od->info_written) {
        assert(bufSize >= ObjectInfo::SIZE);

        memcpy(buf, od->info_str.data(), ObjectInfo::SIZE);
        od->info_written = true;
        return ObjectInfo::SIZE;
    }
    else {
        size_t to_copy = MIN(bufSize, od->payload.size() - od->offset);

        memcpy(buf, &od->payload[od->offset], to_copy);
        od->offset += to_copy;

        return to_copy;
    }
}

bool S3BackupService::putObj(const std::string &key, Object::sp obj)
{
    _putObjData data;
    data.info_written = false;
    data.info_str = obj->getInfo().toString();
    data.payload = obj->getPayload();
    data.offset = 0;

    S3PutObjectHandler handler;
    handler.responseHandler.propertiesCallback = _ignoreRespPropsCB;
    handler.responseHandler.completeCallback = _setStructStatusCB<_putObjData>;
    handler.putObjectDataCallback = _putObjCB;

    S3_put_object(ctx.get(), key.c_str(),
            obj->getInfo().payload_size + ObjectInfo::SIZE,
            NULL, NULL, &handler, &data);

    if (data.status != S3StatusOK) {
        fprintf(stderr, "%s\n", S3_get_status_name(data.status));
        return false;
    }

    return true;
}

