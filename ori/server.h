#ifndef __SERVER_H__
#define __SERVER_H__

#include <microhttpd.h>

#include "repo.h"

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

#endif
