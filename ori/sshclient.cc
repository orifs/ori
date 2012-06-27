#include <cstdlib>
#include <cstdio>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "util.h"

#define D_READ 0
#define D_WRITE 1

#define ORI_LOCATION "/Users/fyhuang/Projects/ori/build/ori/ori"

int start_ssh_connection(const char *remote_host,
        int *fd_to_child, int *fd_from_child)
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

    int childPid = fork();
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

        printf("Hello\n");
        fflush(stdout);

        // This is needed for SSH to function properly (according to rsync)
        Util_SetBlocking(STDIN_FILENO, true);

        // Run ssh
        execlp("/usr/bin/ssh", "ssh", remote_host, ORI_LOCATION, "sshserver", "/Users/fyhuang/Projects/ori/tr", NULL);//, etc.
        //execlp(ORI_LOCATION, "ori", "sshserver", NULL);//, etc.
        perror("execlp failed");
        return -errno;
    }

    // This is the parent
    if (close(pipe_to_child[D_READ]) < 0 ||
            close(pipe_from_child[D_WRITE]) < 0) {
        perror("close");
        exit(errno);
    }

    // TODO: sync here

    *fd_to_child = pipe_to_child[D_WRITE];
    *fd_from_child = pipe_from_child[D_READ];
    return childPid;
}

int cmd_sshclient(int argc, const char *argv[]) {
    if (argc < 2) {
        printf("Usage: ori sshclient [username@]hostname\n");
        exit(1);
    }

    int fd_from_child, fd_to_child;
    int childPid = start_ssh_connection(argv[1], &fd_to_child, &fd_from_child);
    if (childPid < 0) {
        return errno;
    }

    // TODO: this is a stupid synchronization mechanism
    sleep(2);

    // SSH sets stderr to nonblock, possibly screwing up stdout (according to rsync)
    Util_SetBlocking(STDERR_FILENO, true);

    while (true) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(fd_from_child, &read_set);
        FD_SET(STDIN_FILENO, &read_set);

        int ret = select(fd_from_child+1, &read_set, NULL, NULL, NULL);
        if (ret > 0) {
            char buf[512];

            if (FD_ISSET(STDIN_FILENO, &read_set)) {
                size_t read_n = read(STDIN_FILENO, buf, 512);
                write(fd_to_child, buf, read_n);
                fsync(fd_to_child);
            }

            if (FD_ISSET(fd_from_child, &read_set)) {
                // Ready to read from child
                ssize_t read_n = read(fd_from_child, buf, 512);
                if (read_n < 0) perror("read");
                write(STDOUT_FILENO, buf, read_n);
                fflush(stdout);
                fsync(STDOUT_FILENO);
            }
        }
        else if (ret == -1) {
            perror("select");
        }
    }

    printf("SSH connection terminated.\n");
    return 0;
}
