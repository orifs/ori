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

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>

#include <string>
#include <iostream>

using namespace std;

#include "debug.h"
#include "repo.h"
#include "localrepo.h"
#include "server.h"
#include "fuse_cmd.h"

LocalRepo repository;

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
    int (*cmd)(int argc, const char *argv[]);
    void (*usage)(void);
} Cmd;

// General Operations
int cmd_addkey(int argc, const char *argv[]);
#ifdef WITH_LIBS3
int cmd_backup(int argc, const char *argv[]);
#endif /* WITH_LIBS3 */
int cmd_branches(int argc, const char *argv[]);
int cmd_branch(int argc, const char *argv[]);
int cmd_checkout(int argc, const char *argv[]);
int cmd_clone(int argc, const char *argv[]);
void usage_commit(void);
int cmd_commit(int argc, const char *argv[]);
int cmd_filelog(int argc, const char *argv[]);
int cmd_findheads(int argc, const char *argv[]);
int cmd_gc(int argc, const char *argv[]);
void usage_graft(void);
int cmd_graft(int argc, const char *argv[]);
int cmd_init(int argc, const char *argv[]);
int cmd_listkeys(int argc, const char *argv[]);
int cmd_log(int argc, const char *argv[]);
int cmd_merge(int argc, const char *argv[]);
int cmd_pull(int argc, const char *argv[]);
int cmd_rebuildindex(int argc, const char *argv[]);
int cmd_rebuildrefs(int argc, const char *argv[]);
int cmd_remote(int argc, const char *argv[]);
int cmd_removekey(int argc, const char *argv[]);
int cmd_setkey(int argc, const char *argv[]);
int cmd_show(int argc, const char *argv[]);
int cmd_snapshot(int argc, const char *argv[]);
int cmd_snapshots(int argc, const char *argv[]);
int cmd_status(int argc, const char *argv[]);
int cmd_tip(int argc, const char *argv[]);
int cmd_treediff(int argc, const char *argv[]);
int cmd_verify(int argc, const char *argv[]);

// Debug Operations
int cmd_catobj(int argc, const char *argv[]); // Debug
int cmd_listobj(int argc, const char *argv[]); // Debug
int cmd_refcount(int argc, const char *argv[]); // Debug
int cmd_stats(int argc, const char *argv[]); // Debug
int cmd_purgeobj(int argc, const char *argv[]); // Debug
int cmd_purgecommit(int argc, const char *argv[]);
int cmd_stripmetadata(int argc, const char *argv[]); // Debug
int cmd_sshserver(int argc, const char *argv[]); // Internal
int cmd_sshclient(int argc, const char *argv[]); // Debug
#if !defined(WITHOUT_MDNS)
int cmd_mdnsserver(int argc, const char *argv[]); // Debug
#endif
int cmd_httpclient(int argc, const char *argv[]); // Debug
static int cmd_help(int argc, const char *argv[]);
static int cmd_selftest(int argc, const char *argv[]);

static Cmd commands[] = {
    {
        "addkey",
        "Add a trusted public key to the repository",
        cmd_addkey,
        NULL
    },
#ifdef WITH_LIBS3
    {
        "backup",
        "Backup or restore the repository",
        cmd_backup,
        NULL
    },
#endif /* WITH_LIBS3 */
    {
        "branch",
        "Set or print current branch",
        cmd_branch,
        NULL,
    },
    {
        "branches",
        "List all available branches",
        cmd_branches,
        NULL,
    },
    {
	"checkout",
	"Checkout a revision of the repository",
	cmd_checkout,
	NULL,
    },
    {
	"clone",
	"Clone a repository into the local directory",
	cmd_clone,
	NULL,
    },
    {
	"commit",
	"Commit changes into the repository",
	cmd_commit,
	usage_commit,
    },
    {
	"filelog",
	"Display a log of commits to the repository for the specified file",
	cmd_filelog,
	NULL,
    },
    {
        "findheads",
        "Find lost heads",
        cmd_findheads,
        NULL,
    },
    {
	"gc",
	"Reclaim unused space",
	cmd_gc,
	NULL,
    },
    {
	"graft",
	"Graft a subtree from a repository into the local repository",
	cmd_graft,
	usage_graft,
    },
    {
        "help",
        "Show help for a given topic",
        cmd_help,
        NULL
    },
    {
        "init",
        "Initialize the repository",
        cmd_init,
        NULL,
    },
    {
	"listkeys",
	"Display a list of trusted public keys",
	cmd_listkeys,
	NULL,
    },
    {
	"log",
	"Display a log of commits to the repository",
	cmd_log,
	NULL,
    },
    {
	"merge",
	"Merge two heads",
	cmd_merge,
	NULL,
    },
    {
	"pull",
	"Pull changes from a repository",
	cmd_pull,
	NULL,
    },
    {
	"purgecommit",
	"Purge commit",
	cmd_purgecommit,
	NULL,
    },
    {
        "rebuildindex",
        "Rebuild index",
        cmd_rebuildindex,
        NULL,
    },
    {
        "rebuildrefs",
        "Rebuild references",
        cmd_rebuildrefs,
        NULL,
    },
    {
	"remote",
	"Remote connection management",
	cmd_remote,
	NULL,
    },
    {
	"removekey",
	"Remove a public key from the repository",
	cmd_removekey,
	NULL,
    },
    {
        "setkey",
        "Set the repository private key for signing commits",
        cmd_setkey,
        NULL
    },
    {
        "show",
        "Show repository information",
        cmd_show,
        NULL,
    },
    {
	"snapshot",
	"Create a repository snapshot",
	cmd_snapshot,
	NULL,
    },
    {
	"snapshots",
	"List all snapshots available in the repository",
	cmd_snapshots,
	NULL,
    },
    {
        "status",
        "Scan for changes since last commit",
        cmd_status,
        NULL,
    },
    {
        "tip",
        "Print the latest commit on this branch",
        cmd_tip,
        NULL,
    },
    {
        "treediff",
        "Compare two commits",
        cmd_treediff,
        NULL,
    },
    {
	"verify",
	"Verify the repository",
	cmd_verify,
	NULL,
    },
    /* Internal (always hidden) */
    {
        "sshserver",
	NULL, // "Run a simple stdin/out server, intended for SSH access",
        cmd_sshserver,
        NULL
    },
    /* Debugging */
    {
	"catobj",
	"Print an object from the repository (DEBUG)",
	cmd_catobj,
	NULL,
    },
    {
	"listobj",
	"List objects (DEBUG)",
	cmd_listobj,
	NULL,
    },
    {
	"refcount",
	"Print the reference count for all objects (DEBUG)",
	cmd_refcount,
	NULL,
    },
    {
	"stats",
	"Print repository statistics (DEBUG)",
	cmd_stats,
	NULL,
    },
    {
	"purgeobj",
	"Purge object (DEBUG)",
	cmd_purgeobj,
	NULL,
    },
    {
        "stripmetadata",
        "Strip all object metadata including backreferences (DEBUG)",
        cmd_stripmetadata,
        NULL,
    },
    {
        "httpclient",
        "Connect to a server via HTTP (DEBUG)",
        cmd_httpclient,
        NULL
    },
    {
        "sshclient",
        "Connect to a server via SSH (DEBUG)",
        cmd_sshclient,
        NULL
    },
#if !defined(WITHOUT_MDNS)
    {
        "mdnsserver",
        "Run the mDNS server (DEBUG)",
        cmd_mdnsserver,
        NULL
    },
#endif
    {
        "selftest",
        "Built-in unit tests (DEBUG)",
        cmd_selftest,
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

int util_selftest(void);
int LRUCache_selfTest(void);
int Key_selfTest(void);

static int
cmd_selftest(int argc, const char *argv[])
{
    int result = 0;
    result += util_selftest();
    result += LRUCache_selfTest();
    result += Key_selfTest();

    if (result != 0) {
        cout << -result << " errors occurred." << endl;
    }

    return 0;
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

    if (argc == 1) {
        return cmd_help(0, NULL);
    }

    // Open the repository for all command except the following
    bool has_repo = false;
    if (strcmp(argv[1], "clone") != 0 &&
        strcmp(argv[1], "help") != 0 &&
        strcmp(argv[1], "init") != 0 &&
        strcmp(argv[1], "selftest") != 0 &&
        strcmp(argv[1], "sshserver") != 0)
    {
        if (repository.open()) {
            has_repo = true;
            if (ori_open_log(&repository) < 0) {
                printf("Couldn't open log!\n");
                exit(1);
            }
        }
    }
    else {
        has_repo = true;
    }

    if (strcmp(argv[1], "status") == 0 ||
        strcmp(argv[1], "commit") == 0) {
        if (OF_ControlPath().size() > 0)
            has_repo = true;
    }

    if (strcmp(argv[1], "clone") != 0 &&
        strcmp(argv[1], "help") != 0 &&
        strcmp(argv[1], "init") != 0 &&
        strcmp(argv[1], "selftest") != 0 &&
        strcmp(argv[1], "sshserver") != 0)
    {
	if (!has_repo) {
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
    return commands[idx].cmd(argc-1, (const char **)argv+1);
}

