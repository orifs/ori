#ifndef _COMMANDS_H_
#define _COMMANDS_H_
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
    int (*cmd)(int argc, const char *argv);
    void (*usage)(void);
    int flags;
    int argc;
    const char *msg;
} Cmd;

// General Operations
int start_server();
int cmd_init(int argc, const char *argv);
int cmd_add(int argc, const char *argv);
int cmd_remove(int argc, const char *argv);
int cmd_list(int argc, const char *argv);
int cmd_hostadd(int argc, const char *argv);
int cmd_hostremove(int argc, const char *argv);
int cmd_hosts(int argc, const char *argv);
int cmd_help(int argc, const char *argv);
int cmd_foreground(int argc, const char *argv);
int lookupcmd(const char *cmd);

static Cmd commands[] = {
    {
        "init",
        "Interactively configure OriSync",
        cmd_init,
        NULL,
        0,
        0,
        "initialize cluster\nusage: orisync init\n",
    },
    {
        "add",
        "Add a repository to manage",
        cmd_add,
        NULL,
        0,
        2,
        "specify a repository to add\nusage: orisync add <repository>\n",
    },
    {
        "remove",
        "Remove a repository from OriSync",
        cmd_remove,
        NULL,
        0,
        2,
        "Sepcify a repository to remove\nusage: orisync remove <repository>\n",
    },
    {
        "list",
        "List registered repositories",
        cmd_list,
        NULL,
        0,
        0,
        "list registered repsotories\nusage: orisync list\n",
    },
    {
        "hostadd",
        "Add static host",
        cmd_hostadd,
        NULL,
        0,
        2,
        "Sepcify a host to add\nusage: orisync hostadd <HOSTNAME>\n",
    },
    {
        "hostremove",
        "Remove static host",
        cmd_hostremove,
        NULL,
        0,
        2,
        "Sepcify a static host to remove\nusage: orisync hostremove <HOSTNAME>\n",
    },
    {
        "hosts",
        "List static hosts",
        cmd_hosts,
        NULL,
        0,
        0,
        "list static hosts\nusage: orisync hosts\n",
    },
    {
        "help",
        "Display the help message",
        cmd_help,
        NULL,
        0,
        0,
        "",
    },
    {
        "foreground",
        "Foreground mode for debugging",
        cmd_foreground,
        NULL,
        CMD_DEBUG,
        0,
        "",
    },
    { NULL, NULL, NULL, NULL }
};

#endif
