#include <map>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "server.h"
#include "util.h"

typedef std::map<std::string, std::string> ArgsMap;

int print_keys(void *cls,
               enum MHD_ValueKind kind,
               const char *key,
               const char *value)
{
    ArgsMap &args = *(ArgsMap*)cls;
    args[key] = value;
    return MHD_YES;
}

int handle_conn(void *cls,
                struct MHD_Connection *conn,
                const char *url,
                const char *method,
                const char *version,
                const char *upload_data, size_t *upload_data_size,
                void **con_cls) {

    printf("url: %s\n", url);
    std::string command = url+1;

    ArgsMap args;
    MHD_get_connection_values(conn, MHD_GET_ARGUMENT_KIND, &print_keys, &args);


    if (command == "get_object") {
        ArgsMap::iterator it = args.find("hash");
        if (it == args.end()) {
            // TODO queue 400 response
        }
    }

    /*const char *page = "Hello, world!";
    struct MHD_Response *response = MHD_create_response_from_buffer(
            strlen(page),
            (void *)page,
            MHD_RESPMEM_PERSISTENT);

    int ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
    MHD_destroy_response(response);*/

    return MHD_NO;
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



SshServer::SshServer()
{
}

void SshServer::serve(Repo *repo) {
    while (true) {
        char line[256];
        //char c = fgetc(stdin);
        char *status = fgets(line, 256, stdin);
        if (status == NULL) {
            printf("No line\n");
            sleep(1);
        }
        else {
            fputs("Test message", stdout);
            //printf("%c\n", c);
            fputs(line, stdout);
        }
        fflush(stdout);
    }
}

int cmd_sshserver(int argc, const char *argv[])
{
    Util_SetBlocking(STDIN_FILENO, true);
    // TODO: necessary?
    Util_SetBlocking(STDOUT_FILENO, false);

    SshServer server;
    server.serve(NULL); // TODO
    return 0;
}
