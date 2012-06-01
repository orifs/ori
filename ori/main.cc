/*
 * Copyright (c) 2012 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _POSIX_C_SOURCE 200809

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>

#ifdef __MACH__
#include <mach/mach.h>
#include <mach/clock.h>
#endif

#include <string>

using namespace std;

#include "debug.h"
#include "repo.h"

/********************************************************************
 *
 *
 * Command Infrastructure
 *
 *
 ********************************************************************/

typedef struct Cmd {
    const char *name;
    const char *desc;
    int (*cmd)(int argc, char *argv[]);
    void (*usage)(void);
} Cmd;

// repo.cc
int cmd_init(int argc, char *argv[]);
int cmd_show(int argc, char *argv[]);
int cmd_commit(int argc, char *argv[]);
int cmd_checkout(int argc, char *argv[]);
int cmd_status(int argc, char *argv[]);
int cmd_log(int argc, char *argv[]);
int cmd_catobj(int argc, char *argv[]);
int cmd_listobj(int argc, char *argv[]);
// local
static int cmd_help(int argc, char *argv[]);

static Cmd commands[] = {
    {
        "init",
        "Initialize the repository",
        cmd_init,
        NULL,
    },
    {
        "show",
        "Show repository information",
        cmd_show,
        NULL,
    },
    {
        "status",
        "Scan directories recursively for changes since last sync.",
        cmd_status,
        NULL,
    },
    {
	"commit",
	"Commit changes into the repository",
	cmd_commit,
	NULL,
    },
    {
	"checkout",
	"Checkout a revision of the repository",
	cmd_checkout,
	NULL,
    },
    {
	"log",
	"Display a log of commits to the repository",
	cmd_log,
	NULL,
    },
    /* Debugging */
    {
	"catobj",
	"Print an object from the repository",
	cmd_catobj,
	NULL,
    },
    {
	"listobj",
	"List objects",
	cmd_listobj,
	NULL,
    },
    {
        "help",
        "Show help for a given topic",
        cmd_help,
        NULL
    },
    { NULL, NULL, NULL, NULL }
};

static int
lookupcmd(const char *cmd)
{
    int i;

    for (i = 0; commands[i].name != NULL; i++)
    {
        if (strcmp(commands[i].name, cmd) == 0)
            return i;
    }

    return -1;
}

/********************************************************************
 *
 *
 * Logging
 *
 *
 ********************************************************************/

static int logfd = -1;

void
get_timespec(struct timespec *ts)
{
#ifdef __MACH__
    clock_serv_t clk;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &clk);
    clock_get_time(clk, &mts);
    mach_port_deallocate(mach_task_self(), clk);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
#else
    clock_gettime(CLOCK_REALTIME, ts);
#endif
}

void
ori_log(const char *fmt, ...)
{
    va_list ap;
    struct timespec ts;
    char buf[32];

    if (logfd == -1)
        return;

    get_timespec(&ts);
    strftime(buf, 32, "%Y-%m-%d %H:%M:%S ", localtime(&ts.tv_sec));
    dprintf(logfd, "%s", buf);

    va_start(ap, fmt);
    vdprintf(logfd, fmt, ap);
    va_end(ap);

    // XXX: May cause performance issues disable on release builds
    fsync(logfd);
}

static int
cmd_help(int argc, char *argv[])
{
    int i = 0;

    if (argc >= 2) {
        i = lookupcmd(argv[1]);
        if (i != -1 && commands[i].usage != NULL) {
            commands[i].usage();
            return 0;
        }
        if (i == -1) {
            printf("Unknown command '%s'\n", argv[1]);
        } else {
            printf("No help for command '%s'\n", argv[1]);
            return 0;
        }
    }


    for (i = 0; commands[i].name != NULL; i++)
    {
        if (commands[i].desc != NULL)
            printf("%-20s %s\n", commands[i].name, commands[i].desc);
    }

    return 0;
}


int
main(int argc, char *argv[])
{
    int idx;
    string logPath = Repo::getLogPath();

    if (logPath.compare("") != 0) {
        logfd = open(logPath.c_str(), O_CREAT|O_WRONLY|O_APPEND, 0660);
        if (logfd == -1) {
            perror("open");
            return 1;
        }
    } else {
        logfd = -1;
    }

    if (argc == 1) {
        return cmd_help(0, NULL);
    }

    // Open the repository for all command except the following
    if (strcmp(argv[1], "clone") != 0 &&
        strcmp(argv[1], "help") != 0 &&
        strcmp(argv[1], "init") != 0 &&
        strcmp(argv[1], "sshserver"))
    {
        if (!repository.open()) {
            printf("No repository found!\n");
            exit(1);
        }
    }

    idx = lookupcmd(argv[1]);
    if (idx == -1) {
        printf("Unknown command '%s'\n", argv[1]);
        cmd_help(0, NULL);
        return 1;
    }

    LOG("Executing '%s'", argv[1]);
    commands[idx].cmd(argc-1, argv+1);

    return 0;
}

