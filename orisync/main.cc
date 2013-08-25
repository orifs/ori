/*
 * Copyright (c) 2012-2013 Stanford University
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <string>
#include <iostream>

using namespace std;

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/orifile.h>
#include <ori/version.h>
#include <ori/localrepo.h>

#define ORISYNC_LOGFILE         "/.ori/orisync.log"

/********************************************************************
 *
 *
 * Command Infrastructure
 *
 *
 ********************************************************************/

#define CMD_DEBUG               1

typedef struct Cmd {
    const char *name;
    const char *desc;
    int (*cmd)(int argc, const char *argv[]);
    void (*usage)(void);
    int flags;
} Cmd;

// General Operations
int start_server();
int cmd_init(int argc, const char *argv[]);
int cmd_add(int argc, const char *argv[]);
int cmd_remove(int argc, const char *argv[]);
int cmd_list(int argc, const char *argv[]);
static int cmd_help(int argc, const char *argv[]);
// Debug Operations

static Cmd commands[] = {
    {
        "init",
        "Interactively configure OriSync",
        cmd_init,
        NULL,
        0,
    },
    {
        "add",
        "Add a repository to manage",
        cmd_add,
        NULL,
        0,
    },
    {
        "remove",
        "Remove a repository from OriSync",
        cmd_remove,
        NULL,
        0,
    },
    {
        "list",
        "List registered repositories",
        cmd_list,
        NULL,
        0,
    },
    {
        "help",
        "Display the help message",
        cmd_help,
        NULL,
        0,
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

static int
cmd_help(int argc, const char *argv[])
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
        }
        return 0;
    }

    printf("OriSync (%s) - Command Line Interface\n\n",
            ORI_VERSION_STR);
    printf("Available commands:\n");
    for (i = 0; commands[i].name != NULL; i++)
    {
#ifndef DEBUG
        if (commands[i].flags & CMD_DEBUG)
            continue;
#endif /* DEBUG */
        if (commands[i].desc != NULL)
            printf("%-15s %s\n", commands[i].name, commands[i].desc);
    }

    printf("\nPlease report bugs to orifs-devel@stanford.edu\n");
    printf("Website: http://ori.scs.stanford.edu/\n");

    return 0;
}


int
main(int argc, char *argv[])
{
    int idx;

    if (argc == 1) {
        if (!OriFile_Exists(Util_GetHome() + "/.ori"))
            OriFile_MkDir(Util_GetHome() + "/.ori");

        ori_open_log(Util_GetHome() + ORISYNC_LOGFILE);

        return start_server();
    }

    idx = lookupcmd(argv[1]);
    if (idx == -1) {
        printf("Unknown command '%s'\n", argv[1]);
        cmd_help(0, NULL);
        return 1;
    }

    return commands[idx].cmd(argc-1, (const char **)argv+1);
}

