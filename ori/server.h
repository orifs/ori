#ifndef __SERVER_H__
#define __SERVER_H__

#define ORI_PROTO_VERSION "1.0"

#include "object.h"

class SshServer
{
public:
    SshServer();

    void serve();

private:
    void _writeObjectInfo(const ObjectInfo &info);
};

#endif
