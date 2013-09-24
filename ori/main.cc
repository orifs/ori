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
#include <errno.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>

#include <string>
#include <iostream>

using namespace std;

#include <ori/version.h>
#include <oriutil/debug.h>
#include <ori/repo.h>
#include <ori/udsclient.h>
#include <ori/udsrepo.h>
#include <ori/server.h>

#include "fuse_cmd.h"

UDSClient *client;
UDSRepo repository;

/********************************************************************
 *
 *
 * Command Infrastructure
 *
 *
 ********************************************************************/

#define CMD_NEED_FUSE           1
#define CMD_DEBUG               2

typedef struct Cmd {
    const char *name;
    const char *desc;
    int (*cmd)(int argc, char * const argv[]);
    void (*usage)(void);
    int flags;
} Cmd;

// General Operations
int cmd_addkey(int argc, char * const argv[]);
int cmd_branches(int argc, char * const argv[]);
int cmd_branch(int argc, char * const argv[]);
int cmd_checkout(int argc, char * const argv[]);
void usage_cleanup();
int cmd_cleanup(int argc, char * const argv[]);
int cmd_diff(int argc, char * const argv[]);
int cmd_filelog(int argc, char * const argv[]);
int cmd_findheads(int argc, char * const argv[]);
int cmd_gc(int argc, char * const argv[]);
void usage_graft(void);
int cmd_graft(int argc, char * const argv[]);
void usage_list();
int cmd_list(int argc, char * const argv[]);
int cmd_listkeys(int argc, char * const argv[]);
int cmd_log(int argc, char * const argv[]);
int cmd_merge(int argc, char * const argv[]);
void usage_newfs();
int cmd_newfs(int argc, char * const argv[]);
int cmd_pull(int argc, char * const argv[]);
int cmd_rebuildindex(int argc, char * const argv[]);
int cmd_rebuildrefs(int argc, char * const argv[]);
int cmd_remote(int argc, char * const argv[]);
void usage_removefs();
int cmd_removefs(int argc, char * const argv[]);
int cmd_removekey(int argc, char * const argv[]);
void usage_replicate(void);
int cmd_replicate(int argc, char * const argv[]);
int cmd_setkey(int argc, char * const argv[]);
int cmd_show(int argc, char * const argv[]);
void usage_snapshot(void);
int cmd_snapshot(int argc, char * const argv[]);
int cmd_snapshots(int argc, char * const argv[]);
int cmd_status(int argc, char * const argv[]);
int cmd_tip(int argc, char * const argv[]);
int cmd_varlink(int argc, char * const argv[]);

// Debug Operations
int cmd_fsck(int argc, char * const argv[]);
int cmd_purgesnapshot(int argc, char * const argv[]);
int cmd_sshserver(int argc, char * const argv[]); // Internal
static int cmd_help(int argc, char * const argv[]);
static int cmd_version(int argc, char * const argv[]);

static Cmd commands[] = {
    {
        "addkey",
        "Add a trusted public key to the repository (NS)",
        cmd_addkey,
        NULL,
        CMD_DEBUG,
    },
    {
        "branch",
        "Set or print current branch (EXPERIMENTAL) (NS)",
        cmd_branch,
        NULL,
        CMD_DEBUG,
    },
    {
        "branches",
        "List all available branches (EXPERIMENTAL) (NS)",
        cmd_branches,
        NULL,
        CMD_DEBUG,
    },
    {
        "checkout",
        "Checkout a revision of the repository (DEBUG)",
        cmd_checkout,
        NULL,
        CMD_DEBUG | CMD_NEED_FUSE,
    },
    {
        "cleanup",
        "Cleanup a repository after a FUSE crash (DEBUG)",
        cmd_cleanup,
        usage_cleanup,
        CMD_DEBUG,
    },
    {
        "diff",
        "Display a diff of the pending changes (NS)",
        cmd_diff,
        NULL,
        CMD_DEBUG,
    },
    {
        "filelog",
        "Display a log of change to the specified file (NS)",
        cmd_filelog,
        NULL,
        CMD_DEBUG,
    },
    {
        "findheads",
        "Find lost heads (NS)",
        cmd_findheads,
        NULL,
        CMD_DEBUG,
    },
    {
        "gc",
        "Reclaim unused space (NS)",
        cmd_gc,
        NULL,
        CMD_DEBUG,
    },
    {
        "graft",
        "Graft a subtree from a repository into the local repository (NS)",
        cmd_graft,
        usage_graft,
        CMD_DEBUG, /* 0 to allow aliasing of 'cp' */
    },
    {
        "help",
        "Show help for a given topic",
        cmd_help,
        NULL,
        0,
    },
    {
        "list",
        "List local file systems",
        cmd_list,
        usage_list,
        0,
    },
    {
        "listkeys",
        "Display a list of trusted public keys (NS)",
        cmd_listkeys,
        NULL,
        CMD_DEBUG,
    },
    {
        "log",
        "Display a log of commits to the repository",
        cmd_log,
        NULL,
        CMD_NEED_FUSE,
    },
    {
        "merge",
        "Merge two heads",
        cmd_merge,
        NULL,
        CMD_NEED_FUSE,
    },
    {
        "newfs",
        "Create a new file system",
        cmd_newfs,
        usage_newfs,
        0,
    },
    {
        "pull",
        "Pull changes from a repository",
        cmd_pull,
        NULL,
        CMD_NEED_FUSE,
    },
    {
        "purgesnapshot",
        "Purge snapshot (NS)",
        cmd_purgesnapshot,
        NULL,
        CMD_DEBUG,
    },
    {
        "remote",
        "Remote connection management (NS)",
        cmd_remote,
        NULL,
        CMD_DEBUG,
    },
    {
        "removefs",
        "Remove a local replica",
        cmd_removefs,
        usage_removefs,
        0,
    },
    {
        "removekey",
        "Remove a public key from the repository (NS)",
        cmd_removekey,
        NULL,
        CMD_DEBUG,
    },
    {
        "replicate",
        "Create a local replica",
        cmd_replicate,
        usage_replicate,
        0,
    },
    {
        "setkey",
        "Set the repository private key for signing commits (NS)",
        cmd_setkey,
        NULL,
        CMD_DEBUG,
    },
    {
        "show",
        "Show repository information",
        cmd_show,
        NULL,
        CMD_NEED_FUSE,
    },
    {
        "snapshot",
        "Create a repository snapshot",
        cmd_snapshot,
        usage_snapshot,
        CMD_NEED_FUSE,
    },
    {
        "snapshots",
        "List all snapshots available in the repository",
        cmd_snapshots,
        NULL,
        CMD_NEED_FUSE,
    },
    {
        "status",
        "Scan for changes since last commit",
        cmd_status,
        NULL,
        CMD_NEED_FUSE,
    },
    {
        "tip",
        "Print the latest commit on this branch",
        cmd_tip,
        NULL,
        CMD_NEED_FUSE,
    },
    {
        "varlink",
        "Get, set, list varlink variables",
        cmd_varlink,
        NULL,
        CMD_NEED_FUSE,
    },
    /* Internal (always hidden) */
    {
        "sshserver",
        NULL, // "Run a simple stdin/out server, intended for SSH access",
        cmd_sshserver,
        NULL,
        0,
    },
    /* Debugging */
    {
        "fsck",
        "Check internal state of FUSE file system (DEBUG)",
        cmd_fsck,
        NULL,
        CMD_NEED_FUSE | CMD_DEBUG,
    },
    {
        "version",
        "Show version information",
        cmd_version,
        NULL,
        CMD_DEBUG,
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
cmd_help(int argc, char * const argv[])
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

    printf("Ori Distributed Personal File System (%s) - Command Line Interface\n\n",
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

static int
cmd_version(int argc, char * const argv[])
{
    printf("Ori Distributed Personal File System (%s) - Command Line Interface\n",
            ORI_VERSION_STR);
#ifdef GIT_VERSION
    printf("Git Commit Id: " GIT_VERSION "\n");
#endif
#if defined(DEBUG) || defined(ORI_DEBUG)
    printf("Build: DEBUG\n");
#elif defined(ORI_PERF)
    printf("Build: PERF\n");
#else
    printf("Build: RELEASE\n");
#endif

    return 0;
}

int
main(int argc, char *argv[])
{
    int idx;

    if (argc == 1) {
        return cmd_help(0, NULL);
    }

    idx = lookupcmd(argv[1]);
    if (idx == -1) {
        printf("Unknown command '%s'\n", argv[1]);
        cmd_help(0, NULL);
        return 1;
    }

#if defined(DEBUG) || defined(ORI_PERF)
    string logPath = Util_GetHome() + "/.ori/oricli.log";
    ori_open_log(logPath);
#endif

    client = NULL;
    if (commands[idx].flags & CMD_NEED_FUSE) {
        string repoPath;

        if (!OF_HasFuse()) {
            printf("This command must be run on a mounted file system!\n");
            exit(1);
        }

        repoPath = OF_RepoPath();
        if (repoPath == "") {
            printf("Cannot find repository path!\n");
            exit(1);
        }

        try {
            client = new UDSClient(repoPath);
            if (client->connect() < 0) {
                printf("Failed to connect to UDS Repository!\n");
                exit(1);
            }

            repository = UDSRepo(client);
        } catch (exception &e) {
            printf("Failed to connect to FUSE: %s\n", e.what());
            exit(1);
        }
    }

    DLOG("Executing '%s'", argv[1]);
    int status = commands[idx].cmd(argc-1, (char * const*)argv+1);

    if (client)
        delete client;

    return status;
}

