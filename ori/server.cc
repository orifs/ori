
#define _WITH_DPRINTF

#include <cstring>
#include <stdbool.h>
#include <stdint.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <map>

#include "debug.h"
#include "server.h"
#include "util.h"
#include "localrepo.h"

extern LocalRepo repository;

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

    LocalRepoLock::ap lock(repository.lock());
    if (!lock.get()) {
        printf("Couldn't lock repo\n");
        exit(1);
    }

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
                fflush(stdout);
                fsync(STDOUT_FILENO);
                continue;
            }

            // Separate command and arguments
            args_vec args = sep_args(command);
            LOG("sshserver: command '%s'", command);
            
            if (strcmp(command, "hello") == 0) {
                // Setup comm protocol
                printf("%s\nDONE\n", ORI_PROTO_VERSION);
            }
            else if (strcmp(command, "list") == 0) {
                if (strcmp(args[1], "objs") == 0) {
                    std::set<ObjectInfo> objects = repository.listObjects();
                    for (std::set<ObjectInfo>::iterator it = objects.begin();
                            it != objects.end();
                            it++) {
                        _writeObjectInfo(*it);
                    }
                    fflush(stdout);
                    fsync(STDOUT_FILENO);
                    printf("DONE\n");
                }
                else if (strcmp(args[1], "commits") == 0) {
                    const std::vector<Commit> &commits = repository.listCommits();
                    for (size_t i = 0; i < commits.size(); i++) {
                        std::string blob = commits[i].getBlob();
                        dprintf(STDOUT_FILENO, "DATA %lu\n", blob.size());
                        write(STDOUT_FILENO, blob.data(), blob.length());
                    }
                    dprintf(STDOUT_FILENO, "DONE\n");
                }
            }
            else if (strcmp(command, "readobj") == 0) {
                char *hash = args[1];
                // send compressed object
                LocalObject::sp obj = repository.getLocalObject(hash);
                const ObjectInfo &info = obj->getInfo();

                _writeObjectInfo(info);
                printf("DATA %lu\n",
                        obj->getStoredPayloadSize());

                std::auto_ptr<bytestream> bs = obj->getStoredPayloadStream();
                bs->copyToFd(STDOUT_FILENO);
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

void SshServer::_writeObjectInfo(const ObjectInfo &info)
{
    dprintf(STDOUT_FILENO, "%s\n%s\n%08X\n%lu\n",
            info.hash.c_str(),
            Object::getStrForType(info.type),
            info.flags,
            info.payload_size);
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
    Util_SetBlocking(STDOUT_FILENO, true);

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

    if (ori_open_log(&repository) < 0) {
        printf("ERROR couldn't open log\nDONE\n");
        fflush(stdout);
        exit(1);
    }

    SshServer server;
    server.serve();
    return 0;
}
