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

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <ori/debug.h>
#include <ori/oriutil.h>
#include <ori/sshclient.h>
#include <ori/sshrepo.h>

#define D_READ 0
#define D_WRITE 1

/*
 * SshClient
 */
SshClient::SshClient(const std::string &remotePath)
    : fdFromChild(-1), fdToChild(-1), childPid(-1)
{
    ASSERT(Util_IsPathRemote(remotePath));
    size_t pos = remotePath.find(':');
    remoteHost = remotePath.substr(0, pos);
    remoteRepo = remotePath.substr(pos+1);
}

SshClient::~SshClient()
{
    disconnect();
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
    streamToChild.reset(new fdwstream(fdToChild));

    // SSH sets stderr to nonblock, possibly screwing up stdout (according to
    // rsync)
    Util_SetBlocking(STDERR_FILENO, true);

    // Sync by waiting for message from server
    if (!respIsOK()) {
        printf("Couldn't connect to SSH server!\n");
        return -1;
    }

    return 0;
}

void SshClient::disconnect()
{
    if (childPid > 0) {
        //kill(childPid, SIGINT);
        close(fdToChild);
        // Wait for child to die
        while (kill(childPid, 0)) {
            // TODO timeout
        }
    }

    childPid = -1;
}

bool SshClient::connected() {
    return fdFromChild != -1;
}

void SshClient::sendCommand(const std::string &command) {
    ASSERT(connected());
    streamToChild->writePStr(command);
    fsync(fdToChild);
}

void SshClient::sendData(const std::string &data) {
    ASSERT(connected());
    size_t len = data.size();
    size_t off = 0;
    while (len > 0) {
        ssize_t written = write(fdToChild, data.data()+off, len);
        if (written < 0) {
            perror("SshClient::sendData write");
            exit(1);
        }
        len -= written;
        off += written;
    }

    fsync(fdToChild);
}

bytestream *SshClient::getStream() {
    return new fdstream(fdFromChild, -1);
}

bool SshClient::respIsOK() {
    uint8_t resp = 0;
    int status = read(fdFromChild, &resp, 1);
    if (status == 1 && resp == 0) return true;
    else {
        std::string errStr;
        fdstream fs(fdFromChild, -1);
        fs.readPStr(errStr);
        LOG("SSH error: %s", errStr.c_str());
        fprintf(stderr, "SSH error (%d): %s\n", (int)resp, errStr.c_str());
        return false;
    }
}






/*
 * cmd_sshclient
 */
int cmd_sshclient(int argc, const char *argv[]) {
    if (argc < 2) {
        printf("Usage: ori sshclient [username@]hostname\n");
        exit(1);
    }

    std::string remotePath = std::string(argv[1]) +
        ":/Users/fyhuang/Projects/ori/tr3";
    SshClient client(remotePath);
    if (client.connect() < 0) {
        printf("Error connecting to %s\n", argv[1]);
        exit(1);
    }

    printf("Connected\n");

    SshRepo repo(&client);
    std::set<ObjectInfo> objs = repo.listObjects();
    std::vector<ObjectHash> hashes;
    for (std::set<ObjectInfo>::iterator it = objs.begin();
            it != objs.end();
            it++) {
        hashes.push_back((*it).hash);
        printf("Object: %s\n", (*it).hash.hex().c_str());
    }

    fflush(stdout);
    //bytestream *bs = repo.getObjects(hashes);

    // Test getObject
    Object::sp obj(repo.getObject(hashes[0]));
    if (!obj.get()) {
        fprintf(stderr, "Couldn't get object\n");
        return 1;
    }
    bytestream *bs = obj->getPayloadStream();
    std::string payload = bs->readAll();
    printf("Got payload (%lu)\n%s\n", payload.size(), payload.c_str());
    if (bs->error()) {
        fprintf(stderr, "Stream error: %s\n", bs->error());
    }
    printf("Object info:\n");
    obj->getInfo().print();

    sleep(1);

    return 0;
}
