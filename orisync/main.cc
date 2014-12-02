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
#include <sys/socket.h>
#include <sys/un.h>

#include <string>
#include <iostream>

using namespace std;

#include <oriutil/systemexception.h>
#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/orifile.h>
#include <ori/version.h>
#include <ori/localrepo.h>
#include <ori/orisyncclient.h>

#include "commands.h"

#define ORISYNC_LOGFILE         "/.ori/orisync.log"


int
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

int
cmd_help(int argc, const char *argv)
{
    int i = 0;

    if (argc >= 2) {
        i = lookupcmd(argv);
        if (i != -1 && commands[i].usage != NULL) {
            commands[i].usage();
            return 0;
        }
        if (i == -1) {
            printf("Unknown command '%s'\n", argv);
        } else {
            printf("No help for command '%s'\n", argv);
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
cmd_foreground(int argc, const char *argv)
{
    string oriHome = Util_GetHome() + "/.ori";

    if (!OriFile_Exists(oriHome))
        OriFile_MkDir(oriHome);

    // Chdir so that coredumps are placed in ~/.ori
    chdir(oriHome.c_str());

    ori_open_log(Util_GetHome() + ORISYNC_LOGFILE);

    return start_server();
}

/*
 * OrisyncClient
 */
OrisyncClient::OrisyncClient()
{
}

OrisyncClient::OrisyncClient(const string &udsPath)
    : fd(-1)
{
    orisyncPath = udsPath;
}

OrisyncClient::~OrisyncClient()
{
    disconnect();
}

int OrisyncClient::connect()
{
    int status, sock;
    socklen_t len;
    struct sockaddr_un remote;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
        return -1;
        //throw SystemException();

    memset(&remote, 0, sizeof(remote));
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, orisyncPath.c_str());
    len = SUN_LEN(&remote);
    status = ::connect(sock, (struct sockaddr *)&remote, len);
    if (status < 0)
        return -1;
        //throw SystemException();

    fd = sock;
    streamToChild.reset(new fdwstream(fd));

    // Sync by waiting for message from server
    if (!respIsOK()) {
        WARNING("Couldn't connect to UDS server!");
        return -1;
    }

    return 0;
}

void OrisyncClient::disconnect()
{
    close(fd);

    fd = -1;
}

bool OrisyncClient::connected() {
    return fd != -1;
}

/*
void OrisyncClient::sendCommand(const string &command) {
    ASSERT(connected());
    streamToChild->writePStr(command);
}
*/

void OrisyncClient::sendData(const string &data) {
    ASSERT(connected());
    size_t len = data.size();
    size_t off = 0;
    while (len > 0) {
        ssize_t written = write(fd, data.data()+off, len);
        if (written < 0) {
            perror("OrisyncClient::sendData write");
            exit(1);
        }
        len -= written;
        off += written;
    }
}

bytestream *OrisyncClient::getStream() {
    return new fdstream(fd, -1);
}

bool OrisyncClient::respIsOK() {
    uint8_t resp = 0;
    int status = read(fd, &resp, 1);
    if (status == 1 && resp == 0) return true;
    else {
        string errStr;
        fdstream fs(fd, -1);
        fs.readPStr(errStr);
        WARNING("UDS error (%d): %s", (int)resp, errStr.c_str());
        return false;
    }
}

int
OrisyncClient::callCmd(const string &cmd, const string &data)
{
    strwstream ss;
    ss.writePStr(cmd);
    ss.writeLPStr(data);
    sendData(ss.str());

    bool ok = respIsOK();
    //bytestream::ap bs(getStream());
    if (ok) {
        //string result;
        //bs->readLPStr(result);
        //return result;
        return 0;
    }

    return -1;
}
    
int
main(int argc, char *argv[])
{
    int idx;
    string oriHome = Util_GetHome() + "/.ori";

    if (argc == 1) {
        pid_t pid;

        if (!OriFile_Exists(oriHome))
            OriFile_MkDir(oriHome);

        // Chdir so that coredumps are placed in ~/.ori
        chdir(oriHome.c_str());

        ori_open_log(Util_GetHome() + ORISYNC_LOGFILE);

        pid = fork();
        if (pid == -1) {
            perror("fork");
            return 1;
        }
        if (pid > 0) {
            printf("OriSync started as pid %d\n", pid);
            return 0;
        }

        return start_server();
    }

    idx = lookupcmd(argv[1]);
    if (idx == -1) {
        printf("Unknown command '%s'\n", argv[1]);
        cmd_help(0, NULL);
        return 1;
    }

    if (commands[idx].argc == 0)
        return commands[idx].cmd(0, ((const char **)argv+1)[1]);

    if (argc-1 != commands[idx].argc)
    {
        cout << commands[idx].msg;
        cout << argc << endl;
        cout << commands[idx].argc << endl;
        return 0;
    }

    OrisyncClient client(oriHome + "/orisyncSock");
    commands[idx].cmd(0, ((const char**)argv+1)[1]);
    if (client.connect() == 0)
        return client.callCmd(commands[idx].name, ((const char **)argv+1)[1]);
}

