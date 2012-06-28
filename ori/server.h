#ifndef __SERVER_H__
#define __SERVER_H__

#include <microhttpd.h>

#include "repo.h"

#define ORI_PROTO_VERSION "1.0"

class HttpServer
{
public:
    HttpServer(int port);

    void start(Repo *repo);
    void stop();

private:
    int port;
    struct MHD_Daemon *daemon;
};

class SshServer
{
public:
    SshServer();

    void serve();
};

#endif
