#include <map>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "debug.h"
#include "server.h"
#include "util.h"
#include "localrepo.h"

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
            if (command[0] == '\n') {
                // blank line
                printf("DONE\n");
                continue;
            }

            // Separate command and arguments
            args_vec args = sep_args(command);
            LOG("sshserver: command '%s'", command);
            
            if (strcmp(command, "hello") == 0) {
                // Setup comm protocol
                printf("%s\nDONE\n", ORI_PROTO_VERSION);
            }
            else if (strcmp(command, "listobj") == 0) {
                std::set<std::string> objects = repository.listObjects();
                for (std::set<std::string>::iterator it = objects.begin();
                        it != objects.end();
                        it++) {
                    puts((*it).c_str());
                }
                printf("DONE\n");
            }
            else if (strcmp(command, "readobj") == 0) {
                char *hash = args[1];
                // send compressed object
                Object obj = repository.getObject(hash);
                Object::ObjectInfo &info = obj.getInfo();

                printf("%s\n%s\n%08X\n%lu\nDATA %lu\n",
                        hash,
                        info.getTypeStr(),
                        info.flags,
                        info.payload_size,
                        obj.getStoredPayloadSize());

                bytestream *bs = obj.getRawPayloadStream();
                while (!bs->ended()) {
                    uint8_t buf[1024];
                    size_t read_n = bs->read(buf, 1024);
                    write(STDOUT_FILENO, buf, read_n);
                }
                write(STDOUT_FILENO, "DONE\n", 5);
            }
            else if (strcmp(command, "show") == 0) {
                // TODO: is this necessary?
                printf("%s\n%s\n%s\n%s\nDONE\n",
                        repository.getRootPath().c_str(),
                        repository.getUUID().c_str(),
                        repository.getVersion().c_str(),
                        repository.getHead().c_str());
            }
            else {
                printf("ERROR unknown command %s\nDONE\n", command);
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
#ifdef __APPLE__
    fcntl(STDOUT_FILENO, F_NOCACHE, 1); // os x
#endif /* __APPLE__ */

    if (argc < 2) {
        printf("ERROR need repository name\nDONE\n");
        fflush(stdout);
        exit(1);
    }
    if (!repository.open(argv[1])) {
        printf("ERROR no repo found\nDONE\n");
        fflush(stdout);
        exit(101);
    }

    ori_open_log(&repository);

    SshServer server;
    server.serve();
    return 0;
}
