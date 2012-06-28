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

typedef std::vector<char *> args_vec;
args_vec sep_args(char *str) {
    size_t len = strlen(str);
    args_vec rval;

    size_t last = 0;
    for (size_t i = 1; i < len; i++) {
        if (str[i] == ' ' || str[i] == '\n') {
            str[i] = '\0';
            rval.push_back(str+last);
            last = i+1;
        }
    }
    //rval.push_back(str+last);

    return rval;
}

void SshServer::serve() {
    printf("READY\n");
    fflush(stdout);
    fsync(STDOUT_FILENO);

    while (true) {
        // Get command
        char command[256];
        char *status = fgets(command, 256, stdin); // TODO: check for ready to read
        if (status == NULL) {
            if (feof(stdin)) break;
            if (ferror(stdin) && errno != EAGAIN) {
                perror("fgets");
                fflush(stdout);
                exit(1);
            }
            else {
                //sleep(1);
            }
        }
        else {
            // Separate command and arguments
            args_vec args = sep_args(command);
            
            if (strcmp(command, "hello") == 0) {
                // Setup comm protocol
                printf("version: %s\nDONE\n", ORI_PROTO_VERSION);
            }
            else if (strcmp(command, "listobj") == 0) {
                std::set<std::string> objects = repository.getObjects();
                for (std::set<std::string>::iterator it = objects.begin();
                        it != objects.end();
                        it++) {
                    puts((*it).c_str());
                }
                printf("DONE\n");
            }
            else if (strcmp(command, "catobj") == 0) {
                char *hash = args[1];
                std::string obj = repository.getObject(hash);
                printf("hash: %s\ndata-length: %lu\nDATA\n", hash, obj.length());
                write(STDOUT_FILENO, obj.data(), obj.length());
                write(STDOUT_FILENO, "\n", 1);
            }
            else if (strcmp(command, "show") == 0) {
                printf("root: %s\nuuid: %s\nversion: %s\nHEAD: %s\nDONE\n",
                        repository.getRootPath().c_str(),
                        repository.getUUID().c_str(),
                        repository.getVersion().c_str(),
                        repository.getHead().c_str());
            }
            else {
                printf("ERROR unknown command %s\n", command);
            }
        }
        fflush(stdout);
        fsync(STDOUT_FILENO);
    }

    fflush(stdout);
    fsync(STDOUT_FILENO);
}

void ae_flush() {
    fflush(stdout);
    fsync(STDOUT_FILENO);
}

int cmd_sshserver(int argc, const char *argv[])
{
    atexit(ae_flush);

    Util_SetBlocking(STDIN_FILENO, true);
    // TODO: necessary?
    Util_SetBlocking(STDOUT_FILENO, false);

    // Disable output buffering
    setvbuf(stdout, NULL, _IONBF, 0); // libc
    fcntl(STDOUT_FILENO, F_NOCACHE, 1); // os x

    if (argc < 2) {
        printf("ERROR need repository name\n");
        fflush(stdout);
        exit(1);
    }
    if (!repository.open(argv[1])) {
        printf("ERROR no repo found\n");
        fflush(stdout);
        exit(101);
    }

    SshServer server;
    server.serve();
    return 0;
}
