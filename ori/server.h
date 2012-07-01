#ifndef __SERVER_H__
#define __SERVER_H__

#include "repo.h"

#define ORI_PROTO_VERSION "1.0"

class SshServer
{
public:
    SshServer();

    void serve();
};

#endif
