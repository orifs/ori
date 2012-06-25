#include "server.h"

int handle_conn(void *cls,
                struct MHD_Connection *conn,
                const char *url,
                const char *method,
                const char *version,
                const char *upload_data, size_t *upload_data_size,
                void **con_cls) {

    printf("url: %s\n", url);

    const char *page = "Hello, world!";
    struct MHD_Response *response = MHD_create_response_from_buffer(
            strlen(page),
            (void *)page,
            MHD_RESPMEM_PERSISTENT);

    int ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

HttpServer::HttpServer(int port)
    : port(port)
{
}

void HttpServer::start(Repo *repo) {
    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, port, NULL, NULL, &handle_conn, repo, MHD_OPTION_END);
}

void HttpServer::stop() {
    MHD_stop_daemon(daemon);
}
