#include <cstdlib>
#include <cstdio>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "sshclient.h"
#include "util.h"

#define D_READ 0
#define D_WRITE 1

/*
 * SshClient
 */
SshClient::SshClient(const std::string &remoteHost, const std::string
        &remoteRepo)
    : remoteHost(remoteHost), remoteRepo(remoteRepo),
      fdFromChild(-1), fdToChild(-1), childPid(-1)
{
}

SshClient::~SshClient()
{
    if (childPid > 0) {
        kill(childPid, SIGINT);
        // Wait for child to die
        while (kill(childPid, 0)) {
        }
    }
}

int SshClient::connect()
{
    int pipe_to_child[2], pipe_from_child[2];
    if (pipe(pipe_to_child) < 0) {
        perror("pipe");
        return -errno;
    }
    if (pipe(pipe_from_child) < 0) {
        perror("pipe");
        return -errno;
    }

    childPid = fork();
    if (childPid < 0) {
        perror("fork");
        return -errno;
    }
    else if (childPid == 0) {
        // This is the child process
        if (close(pipe_to_child[D_WRITE]) < 0 ||
                close(pipe_from_child[D_READ]) < 0) {
            perror("close");
            return -errno;
        }

        // dup pipes into stdin/out
        if (dup2(pipe_to_child[D_READ], STDIN_FILENO) < 0) {
            perror("dup2");
            return -errno;
        }
        if (dup2(pipe_from_child[D_WRITE], STDOUT_FILENO) < 0) {
            perror("dup2");
            return -errno;
        }

        // close pipe fds (already duped)
        if (pipe_to_child[D_READ] != STDIN_FILENO) {
            if (close(pipe_to_child[D_READ]) < 0) {
                perror("close"); return -errno;
            }
        }
        if (pipe_from_child[D_WRITE] != STDOUT_FILENO) {
            if (close(pipe_from_child[D_WRITE]) < 0) {
                perror("close"); return -errno;
            }
        }

        // This is needed for SSH to function properly (according to rsync)
        Util_SetBlocking(STDIN_FILENO, true);

        // Run ssh
        execlp("/usr/bin/ssh", "ssh", remoteHost.c_str(), "ori", "sshserver",
                remoteRepo.c_str(), NULL);
        perror("execlp failed");
        return -errno;
    }

    // This is the parent
    if (close(pipe_to_child[D_READ]) < 0 ||
            close(pipe_from_child[D_WRITE]) < 0) {
        perror("close");
        exit(errno);
    }

    fdToChild = pipe_to_child[D_WRITE];
    fdFromChild = pipe_from_child[D_READ];

    // TODO: sync here
    sleep(2);

    // SSH sets stderr to nonblock, possibly screwing up stdout (according to
    // rsync)
    Util_SetBlocking(STDERR_FILENO, true);

    // Wait for READY from server
    std::string ready;
    recvResponse(ready);
    if (ready != "READY\n") {
        return -1;
    }

    return 0;
}

void SshClient::sendCommand(const std::string &command) {
    write(fdToChild, command.data(), command.length());
    write(fdToChild, "\n", 1);
    fsync(fdToChild);
}

void SshClient::recvResponse(std::string &out) {
    out.clear();
    while (true) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(fdFromChild, &read_set);

        int ret = select(fdFromChild+1, &read_set, NULL, NULL, NULL);
        if (ret == 1) {
            char buf[512];

            // Ready to read from child
            ssize_t read_n = read(fdFromChild, buf, 512);
            if (read_n < 0) {
                perror("read");
                out.clear();
                return;
            }

            printf("Read '%s'\n", buf);

            size_t curr_size = out.size();
            out.resize(curr_size + read_n);
            memcpy(&out[curr_size], buf, read_n);

            if (strcmp(&out[out.size()-5], "DONE\n") == 0) {
                return;
            }
            else if (strcmp(&out[out.size()-6], "READY\n") == 0) {
                return;
            }
        }
        else if (ret == -1) {
            perror("select");
            out.clear();
            return;
        }
    }
}



/*
 * SshRepo
 */



int cmd_sshclient(int argc, const char *argv[]) {
    if (argc < 2) {
        printf("Usage: ori sshclient [username@]hostname\n");
        exit(1);
    }

    SshClient client(argv[1], "/Users/fyhuang/Projects/ori/tr");
    if (client.connect() < 0) {
        printf("Error connecting to %s\n", argv[1]);
        exit(1);
    }

    dprintf(STDOUT_FILENO, "Connected\n");

    // Simple command-line client
    while (true) {
        char buf[256];
        char *status = fgets(buf, 256, stdin);
        if (status == NULL) {
            if (feof(stdin)) break;
            if (ferror(stdin)) {
                perror("fgets");
                break;
            }
        }
        else {
            buf[strlen(buf)-1] = '\0'; // remove trailing \n
            client.sendCommand(buf);
            std::string resp;
            client.recvResponse(resp);
            if (resp.size() == 0) {
                break;
            }
            write(STDOUT_FILENO, resp.data(), resp.size());
        }
    }

    printf("Terminating SSH connection...\n");
    return 0;
}
