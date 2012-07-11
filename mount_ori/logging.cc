#include "ori_fuse.h"

static int logfd = 0;

void
ori_fuse_log(const char *what, ...)
{
    if (logfd == 0) {
        logfd = open("ori_fuse.log", O_CREAT|O_WRONLY|O_TRUNC, 0660);
        if (logfd == -1) {
            perror("open");
            exit(1);
        } 
    }

    va_list vl;
    va_start(vl, what);
    vdprintf(logfd, what, vl);

    fsync(logfd);
}

