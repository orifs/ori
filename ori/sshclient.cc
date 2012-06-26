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

int cmd_sshclient(int argc, const char *argv[]) {
    if (argc < 2) {
        printf("Usage: ori sshclient [username@]hostname\n");
        exit(1);
    }

    int pipe_to_child[2], pipe_from_child[2];
    if (pipe(pipe_to_child) < 0) {
        perror("pipe");
        return errno;
    }
    if (pipe(pipe_from_child) < 0) {
        perror("pipe");
        return errno;
    }

    int childPid = fork();
    if (childPid < 0) {
        perror("fork");
        return errno;
    }
    else if (childPid == 0) {
        // This is the child process
        if (close(pipe_to_child[D_WRITE]) < 0 ||
                close(pipe_from_child[D_READ]) < 0) {
            perror("close");
            exit(errno);
        }

        // dup pipes into stdin/out
        if (dup2(pipe_to_child[D_READ], STDIN_FILENO) < 0) {
            perror("dup2");
            exit(errno);
        }
        if (dup2(pipe_from_child[D_WRITE], STDOUT_FILENO) < 0) {
            perror("dup2");
            exit(errno);
        }

        // close pipe fds (already duped)
        if (pipe_to_child[D_READ] != STDIN_FILENO) {
            if (close(pipe_to_child[D_READ]) < 0) {
                perror("close"); exit(errno);
            }
        }
        if (pipe_from_child[D_WRITE] != STDOUT_FILENO) {
            if (close(pipe_from_child[D_WRITE]) < 0) {
                perror("close"); exit(errno);
            }
        }

        // This is needed for SSH to function properly (according to rsync)
        Util_SetBlocking(STDIN_FILENO, true);
        write(pipe_from_child[D_WRITE], "test", 4);
        fputs("Starting SSH", stdout);
        fflush(stdout);
        fsync(STDOUT_FILENO);


        // Run ssh
        execlp("/usr/bin/ssh", "ssh", argv[1], ORI_LOCATION, "sshserver", NULL);//, etc.
        //execlp(ORI_LOCATION, "ori", "sshserver", NULL);//, etc.
        perror("execlp failed");
        exit(errno);
    }
    else {
        // This is the parent process
        if (close(pipe_to_child[D_READ]) < 0 ||
                close(pipe_from_child[D_WRITE]) < 0) {
            perror("close");
            exit(errno);
        }

        sleep(5);
        // SSH sets stderr to nonblock, possibly screwing up stdout (according to rsync)
        Util_SetBlocking(STDERR_FILENO, true);
        printf("This message should appear\n");
        //printf("Press enter");
        //getchar();

        fd_set read_set;
        struct timeval tv_zero;
        char buf[512];

        while (true) {
            //write(real_stdout, "polling\n", 8);
            tv_zero.tv_sec = 0;
            tv_zero.tv_usec = 0;
            FD_ZERO(&read_set);
            FD_SET(pipe_from_child[D_READ], &read_set);
            FD_SET(STDIN_FILENO, &read_set);

            int ret = select(pipe_from_child[D_READ]+1, &read_set, NULL, NULL, NULL);
            if (ret > 0) {
                if (FD_ISSET(STDIN_FILENO, &read_set)) {
                    puts("Reading input");
                    size_t read_n = read(STDIN_FILENO, buf, 512);
                    write(pipe_to_child[D_WRITE], buf, read_n);
                    fsync(pipe_to_child[D_WRITE]);
                }
                if (FD_ISSET(pipe_from_child[D_READ], &read_set)) {
                    // Ready to read
                    ssize_t read_n = read(pipe_from_child[D_READ], buf, 512);
                    printf("Recv %d: '", read_n);
                    fflush(stdout);
                    write(STDOUT_FILENO, buf, read_n);
                    printf("'\n");
                    fflush(stdout);
                    fsync(STDOUT_FILENO);
                }
            }
            else if (ret == -1) {
                perror("select");
            }
        }
    }
}
