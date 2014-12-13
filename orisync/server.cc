/*
 * Copyright (c) 2013 Stanford University
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

#include <stdint.h>
#include <stdio.h>
#include <cstdlib>

#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <queue>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <sys/un.h>

#include <oriutil/debug.h>
#include <oriutil/oristr.h>
#include <oriutil/oriutil.h>
#include <oriutil/oricrypt.h>
#include <oriutil/orinet.h>
#include <oriutil/systemexception.h>
#include <oriutil/thread.h>
#include <oriutil/kvserializer.h>
#include <ori/localrepo.h>

#include "orisyncconf.h"
#include "repoinfo.h"
#include "hostinfo.h"
#include "repocontrol.h"
#include "commands.h"

using namespace std;

// Announcement UDP port
#define ORISYNC_UDPPORT         8051
// Advertisement interval
#define ORISYNC_ADVINTERVAL     1 //5
// Reject advertisements with large time skew
#define ORISYNC_ADVSKEW         5
// Repository check interval
#define ORISYNC_MONINTERVAL     1 //10
// Repository snapshot interval in slow mode
#define ORISYNC_SLOWSSINTERVAL 30
// Sync interval
#define ORISYNC_SYNCINTERVAL    1 //5
// Watchdog interval
#define ORISYNC_WDINTERVAL      10 // seconds for now
// Garbage collection interval
#define ORISYNC_GCINTERVAL       3600 // seconds for now; How often do we run garbage collection
// Purge time
#define ORISYNC_PURGETIME      36000 // seconds for now; How old will the repo be purged?
// Timeout to detect host down
#define HOST_TIMEOUT            10

#define OK 0
#define ERROR 1

// Turn on time measurement of sync performance
#define TIME_PLOG 0

OriSyncConf rc;
RWLock rcLock;
HostInfo myInfo;
//RWLock infoLock;
map<string, HostInfo *> hosts;
RWLock hostsLock;
struct timespec listentime;
struct mrqElem {
    string uuid;
    string hostId;
};
queue<struct mrqElem> mrq; // Modified Repo Queue
mutex mrqLock;
condition_variable mrqCV;
mutex exitLock;
condition_variable exitCV;

class Listener : public Thread
{
public:
    Listener() : Thread() {
        int status;
        struct sockaddr_in addr;
        int reuseaddr = 1;

        fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (fd < 0) {
            throw SystemException();
        }

        status = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                            &reuseaddr, sizeof(reuseaddr));
        if (status < 0) {
            perror("setsockopt");
            close(fd);
            throw SystemException();
        }
#ifdef __APPLE__
        status = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT,
                            &reuseaddr, sizeof(reuseaddr));
        if (status < 0) {
            perror("setsockopt");
            close(fd);
            throw SystemException();
        }
#endif /* __APPLE__ */

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(ORISYNC_UDPPORT);

        status = ::bind(fd, (struct sockaddr *)&addr, sizeof(addr));
        if (status < 0) {
            perror("bind");
            close(fd);
            throw SystemException();
        }
    }
    ~Listener() {
        close(fd);
    }
    void updateRepoinfo(HostInfo *remote) {
        RWKey::sp rhostKey = remote->hostLock.readLock();
        list<string> remoteList = remote->listRepos();
        // Check if any repo has been removed from remote
        //RWKey::sp key = infoLock.readLock();
        RWKey::sp key = myInfo.hostLock.readLock();
        list<RepoInfo> repoList = myInfo.listRepoInfo();
        for (RepoInfo rInfo : repoList) {
            if (rInfo.hasPeer(remote->getHost()) && (!remote->hasRepo(rInfo.getRepoId()))) {
                myInfo.removeRepoPeer(rInfo.getRepoId(), remote->getHost());
            }
        }
        for (string repoID : remoteList) {
            // Check if local has the repo
            if (!myInfo.hasRepo(repoID)) {
                DLOG("Local info not found for repo %s", repoID.c_str());
                continue;
            }

            // Update peer information
            if (remote->getRepo(repoID).isMounted()) {
                myInfo.insertRepoPeer(repoID, remote->getHost());
            } else {
                myInfo.removeRepoPeer(repoID, remote->getHost());
            }

            // Check if head matches
            if (myInfo.getRepo(repoID).getHead() == remote->getRepo(repoID).getHead())
                continue;

            if (TIME_PLOG) clock_gettime(CLOCK_REALTIME, &listentime);
            // Enqueue repo for Syncer
            struct mrqElem repo;
            repo.uuid = repoID;
            repo.hostId = remote->getHostId();
            {
                lock_guard<mutex> lk(mrqLock);
                mrq.push(repo);
                DLOG("New MRQ elem pushed");
            }
            mrqCV.notify_one();
        }
        key.reset();
        rhostKey.reset();
    }
    void updateHost(KVSerializer &kv, const string &srcIp) {
        RWKey::sp key = hostsLock.writeLock();
        map<string, HostInfo *>::iterator it;
        string hostId = kv.getStr("hostId");
 
        // Update
        it = hosts.find(hostId);
        if (it == hosts.end()) {
            hosts[hostId] = new HostInfo(hostId, kv.getStr("cluster"));
        }
        HostInfo *rhost = hosts[hostId];
        key.reset();
        RWKey::sp key2 = rhost->hostLock.writeLock();
        rhost->update(kv);
        rhost->setPreferredIp(srcIp);
        rhost->setTime(time(NULL));
        rhost->setStatus("OK");
        rhost->setDown(false);
        key2.reset();
        updateRepoinfo(rhost);
    }
    void parse(const char *buf, int len, sockaddr_in *source) {
        string ctxt;
        string ptxt;
        KVSerializer kv;
        char srcip[INET_ADDRSTRLEN];

        // Message too small
        if (len < 32)
            return;

        // Cluster ID
        ctxt.assign(buf, 32);
        ctxt[31] = '\0';
        if (strncmp(ctxt.c_str(), rc.getCluster().c_str(), 31) != 0)
            return;

        ctxt.assign(buf+32, len-32);
        ptxt = OriCrypt_Decrypt(ctxt, rc.getKey());
        try {
            kv.fromBlob(ptxt);
            //kv.dump();

            // Prevent replay attacks from leaking information
            uint64_t now = time(NULL);
            uint64_t ts = kv.getU64("time");
            string hostId = kv.getStr("hostId");
            if (ts > now + ORISYNC_ADVSKEW || ts < now - ORISYNC_ADVSKEW) {
                WARNING("Host %s time out of sync by %d seconds.", hostId.c_str(), (int)(ts - now));
                WARNING("Ignoring host %s", hostId.c_str());
                RWKey::sp key = hostsLock.readLock();
                map<string, HostInfo *>::iterator it;
                it = hosts.find(hostId);
                if (it == hosts.end()) {
                    key.reset();
                    return;
                }
                HostInfo *rhost = hosts[hostId];
                key.reset();

                RWKey::sp key2 = rhost->hostLock.writeLock();
                rhost->setStatus("Time out of sync");
                rhost->setDown(false);
                key2.reset();
                return;
            }

            // Ignore requests from self
            if (hostId == rc.getUUID())
                return;

            // Ignore messages from other clusters
            if (kv.getStr("cluster") != rc.getCluster())
                return;

            if (!inet_ntop(AF_INET, &(source->sin_addr),
                           srcip, INET_ADDRSTRLEN)) {
                LOG("Error parsing source ip");
                return;
            }

            // Add or update hostinfo
            updateHost(kv, srcip);
        } catch(SerializationException e) {
            LOG("Error encountered parsing announcement: %s", e.what());
            return;
        }
    }
    void dumpHosts() {
        RWKey::sp key = hostsLock.readLock();

        cout << "=== Begin Hosts ===" << endl;
        for (auto &it : hosts) {
            cout << it.second->getHost() << endl;
        }
        cout << "==== End Hosts ====" << endl << endl;
    }
    void run() {
        while (!interruptionRequested()) {
            char buf[1500];
            int len = 1500;
            struct sockaddr_in srcAddr;
            socklen_t srcAddrLen = sizeof(srcAddr);

            len = recvfrom(fd, buf, len, 0,
                           (struct sockaddr *)&srcAddr,
                           &srcAddrLen);
            if (len < 0) {
                perror("recvfrom");
                continue;
            }
            parse(buf, len, &srcAddr);

            //dumpHosts();
        }

        DLOG("Listener exited!");
    }
private:
    int fd;
};

class RepoMonitor : public Thread
{
public:
    RepoMonitor() : Thread() {
        int status;
        int broadcast = 1;

        fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (fd < 0) {
            throw SystemException();
        }

        status = setsockopt(fd, SOL_SOCKET, SO_BROADCAST,
                            &broadcast, sizeof(broadcast));
        if (status < 0) {
            close(fd);
            throw SystemException();
        }

        memset(&dstAddr, 0, sizeof(dstAddr));
        dstAddr.sin_family = AF_INET;
        dstAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        dstAddr.sin_port = htons(ORISYNC_UDPPORT);
    }
    ~RepoMonitor() {
        close(fd);
    }
    string generate() {
        //RWKey::sp key = infoLock.readLock();
        RWKey::sp key = myInfo.hostLock.readLock();
        char buf[32];
        string msg;

        // First 31 bytes of cluster with null
        memset(buf, 0, 32);
        strncpy(buf, rc.getCluster().c_str(), 31);
        msg.assign(buf, 32);
        msg.append(OriCrypt_Encrypt(myInfo.getBlob(), rc.getKey()));

        return msg;
    }
    void announce() {
        int status;
        string msg;
        size_t len;

        msg = generate();
        len = msg.size();

        status = sendto(fd, msg.c_str(), len, 0,
                        (struct sockaddr *)&dstAddr,
                        sizeof(dstAddr));
        if (status < 0) {
            perror("sendto");
        }
    }
    /*
     * Update repository information and return thre repoId, otherwise empty 
     * string.
     */
    void updateRepo(const string &path) {
        RWKey::sp key = myInfo.hostLock.writeLock();
        RepoControl repo = RepoControl(path);
        RepoInfo info;
        int ret = 0;;

        try {
            repo.open();
        } catch (SystemException &e) {
            WARNING("Failed to open repository %s: %s", path.c_str(), e.what());
            return;
        }

        //RWKey::sp key = infoLock.writeLock();
        if (myInfo.hasRepo(repo.getUUID())) {
            info = myInfo.getRepo(repo.getUUID());
            if (info.getPath() != path) {
                 info = RepoInfo(repo.getUUID(), repo.getPath(), repo.isMounted());
            }
            info.setMounted(repo.isMounted());
        } else {
            DLOG("New repo added: %s", repo.getPath().c_str());
            info = RepoInfo(repo.getUUID(), repo.getPath(), repo.isMounted());
        }

        struct timespec ts1, ts2, ts3, ts4;
        
        if (TIME_PLOG) clock_gettime(CLOCK_REALTIME, &ts1);

        if (repo.isMounted()) {
            if (!info.hasRemote()) {
                // Take snapshot with longer interval
                if (info.getSStime() > time(NULL) - ORISYNC_SLOWSSINTERVAL)
                    goto skipss;

            }
            RWKey::sp repoKey = myInfo.getRepoLock(repo.getUUID())->writeLock();
            ret = repo.snapshot();
            info.setSStime();
            repoKey.reset();
        }
        if (TIME_PLOG) clock_gettime(CLOCK_REALTIME, &ts2);

skipss:
        //before my change to updateRepo,
        //when local2 is a replica of local 1, and when we check local2, say local 2 
        //has a new commit, but local1 hasn't yet. local1's head will be updated here
        //isn't this a bug??????
        info.updateHead(repo.getHead());
        myInfo.updateRepo(repo.getUUID(), info);
        key.reset();

        //LOG("Checked %s: %s %s", path.c_str(), repo.getHead().c_str(), repo.getUUID().c_str());

        if (ret == 1) {// Repo has changed
          if (TIME_PLOG)clock_gettime(CLOCK_REALTIME, &ts3);
          announce();
          if (TIME_PLOG) {
            clock_gettime(CLOCK_REALTIME, &ts4);
            printf("time1: %ld %ld\n", ts1.tv_sec, ts1.tv_nsec);
            printf("time2: %ld %ld\n", ts2.tv_sec, ts2.tv_nsec);
            printf("time3: %ld %ld\n", ts3.tv_sec, ts3.tv_nsec);
            printf("time4: %ld %ld\n", ts4.tv_sec, ts4.tv_nsec);
          }
        }
        
        repo.close();

        return;
    }
    void run() {

        while (!interruptionRequested()) {
            RWKey::sp key = rcLock.readLock();
            list<string> repos = rc.getRepos();
            key.reset();
            for (auto &it : repos) {
                updateRepo(it);
            }

            announce();
            sleep(ORISYNC_MONINTERVAL);
        }


        DLOG("RepoMonitor exited!");
    }
private:
    int fd;
    struct sockaddr_in dstAddr;
};

class Syncer : public Thread
{
public:
    Syncer() : Thread()
    {
    }
    void pullRepoLocal(RepoInfo &localRepoA,
                       RepoInfo &localRepoB)
    {
        RWKey::sp key = myInfo.hostLock.writeLock();
        RepoControl repo = RepoControl(localRepoA.getPath());

        DLOG("Local and Remote heads mismatch on repo %s", localRepoA.getRepoId().c_str());

        try {
            repo.open();
        } catch (SystemException &e) {
            WARNING("Failed to open repository %s: %s", localRepoA.getPath().c_str(), e.what());
            return;
        }
        bool hasCommit;
        try {
          hasCommit = repo.hasCommit(localRepoB.getHead());
        } catch (SystemException &e) {
            WARNING("%s", e.what());
            repo.close();
            return;
        }
        if (!hasCommit) {
            LOG("Pulling from local repo %s",
                localRepoB.getPath().c_str());
            RWKey::sp repoKey = myInfo.getRepoLock(localRepoA.getRepoId())->writeLock();
            repo.pull("localhost", localRepoB.getPath());
            repoKey.reset();
        }
        repo.close();
    }
    void pullRepo(struct mrqElem &mRepo)
    {
        RWKey::sp key = myInfo.hostLock.writeLock();
        HostInfo localHost = myInfo;
        if (!localHost.hasRepo(mRepo.uuid)) {
            DLOG("Local info not found for repo %s", mRepo.uuid.c_str());
            return;
        }
        RWKey::sp repoKey = myInfo.getRepoLock(mRepo.uuid)->writeLock();
        RepoInfo local = localHost.getRepo(mRepo.uuid);
        RepoControl repo = RepoControl(local.getPath());
        RepoInfo remote;
        string srcPath;

        {
            RWKey::sp key = hostsLock.readLock();
            map<string, HostInfo *>::iterator it;
            it = hosts.find(mRepo.hostId);
            if (it == hosts.end()) {
                DLOG("Host %s not found", mRepo.hostId.c_str());
                return;
            }
            HostInfo *remoteHost = hosts[mRepo.hostId];
            if (!remoteHost->hasRepo(mRepo.uuid)) {
                DLOG("Repo %s not found on host %s", mRepo.uuid.c_str(), remoteHost->getHost().c_str());
                return;
            }
            remote = remoteHost->getRepo(mRepo.uuid);
            string username = remoteHost->getUsername();
            srcPath = (username.empty()) ? remoteHost->getPreferredIp() : username + '@' + remoteHost->getPreferredIp();
        }

        DLOG("Local and Remote heads mismatch on repo %s", mRepo.uuid.c_str());

        try {
            repo.open();
        } catch (SystemException &e) {
            WARNING("Failed to open repository %s: %s", local.getPath().c_str(), e.what());
            return;
        }
        bool hasCommit;
        try {
          hasCommit = repo.hasCommit(remote.getHead());
        } catch (SystemException &e) {
            WARNING("%s", e.what());
            repo.close();
            return;
        }
        if (!hasCommit) {
            // TODO: Once garbage collect is on, we can no longer rely on this. If remote has an repo that's too out of date. We might not contain the commit. Need to also check if the remote head time < now - gcinterval
            LOG("Pulling from %s:%s", srcPath.c_str(),
                remote.getPath().c_str());
            struct timespec ts1, ts2;
            if (TIME_PLOG) clock_gettime(CLOCK_REALTIME, &ts1);
            repo.pull(srcPath, remote.getPath());
            if (TIME_PLOG) {
              clock_gettime(CLOCK_REALTIME, &ts2);
              printf("time5: %ld %ld\n", listentime.tv_sec, listentime.tv_nsec);
              printf("time6: %ld %ld\n", ts1.tv_sec, ts1.tv_nsec);
              printf("time7: %ld %ld\n", ts2.tv_sec, ts2.tv_nsec);
            }
        }
        key.reset();
        repoKey.reset();
        repo.close();
    }
    void run() {
        while (!interruptionRequested()) {
            unique_lock<mutex> lk(mrqLock);
            mrqCV.wait_for(lk, chrono::seconds(ORISYNC_SYNCINTERVAL));
            if (interruptionRequested()) {
                lk.unlock();
                break;
            }

            while (!mrq.empty()) {
                struct mrqElem mRepo = mrq.front();
                mrq.pop();
                lk.unlock();
                LOG("Syncer checking %s", mRepo.uuid.c_str());
                pullRepo(mRepo);
                lk.lock();
            }
            lk.unlock();

            HostInfo infoSnapshot = myInfo;
            list<RepoInfo> allrepos = infoSnapshot.listAllRepos();
            for (auto &it : allrepos) {
                //LOG("Syncer local checking %s, %s", it.getRepoId().c_str(), it.getPath().c_str());
                //check local repo
                for (auto &it_cpy : allrepos) {
                      //LOG("local repo check, %s and %s have uuid %s and %s", it.getPath().c_str(),
                      //    it_cpy.getPath().c_str(), it.getRepoId().c_str(), it_cpy.getRepoId().c_str());
                    if (it.getRepoId() == it_cpy.getRepoId()) {
                      //LOG("local repo check, %s and %s have same uuid", it.getPath().c_str(),
                      //    it_cpy.getPath().c_str());
                      if (it.getHead() != it_cpy.getHead()) {
                          LOG("local repo %s and %s don't have the same head",
                              it.getPath().c_str(), it_cpy.getPath().c_str());
                        pullRepoLocal(it, it_cpy);
                      }
                    }
                }
           }
        }

        DLOG("Syncer exited!");
    }
};

void
Httpd_getRoot(struct evhttp_request *req, void *arg)
{
    struct evbuffer *buf;

    buf = evbuffer_new();
    if (buf == NULL) {
        evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
        return;
    }

    evbuffer_add_printf(buf, "Temporary String");
    evhttp_add_header(req->output_headers, "Content-Type", "text/html");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

class UdsServer : public Thread
{
public:
    UdsServer() : Thread()
    {
        int status, sock, len;
        string orisyncSock;
        struct sockaddr_un serAddr;

        string oriHome = Util_GetHome() + "/.ori";
        orisyncSock = oriHome + "/orisyncSock";
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0)
            throw SystemException();

        memset(&serAddr, 0, sizeof(serAddr));
        serAddr.sun_family = AF_UNIX;
        strcpy(serAddr.sun_path, orisyncSock.c_str());
        unlink(orisyncSock.c_str());
        len = SUN_LEN(&serAddr);
        status = ::bind(sock, (struct sockaddr *)&serAddr, len);
        if (status < 0)
            throw SystemException();

        status = listen(sock, 5);
        if (status < 0)
            throw SystemException();

        listenFd = sock;
    }
    ~UdsServer()
    {
        unlink(orisyncSock.c_str());
        close(listenFd);
    }
    void call_cmd(int sock)
    {
        fdstream in(sock, -1);
        string cmd;
        string data;

        in.readPStr(cmd);
        in.readLPStr(data);

        DLOG("callcmd %s", cmd.c_str());
        fdwstream fs(sock);

        int idx = lookupcmd(cmd.c_str());
        commands[idx].cmd(1, data.c_str());
        // Some special handling for remove
        if (cmd == "remove") {
            myInfo.removePath(data);
        }

        fs.writeUInt8(OK);
    }
    void run()
    {   
        int client;
        socklen_t clilen;
        struct sockaddr_un cliAddr;

        clilen = sizeof(cliAddr);

        while (!interruptionRequested()) {
            client = accept(listenFd, (struct sockaddr *)&cliAddr, &clilen);
            if (client < 0) {
                perror("accept: ");
                continue;
            }
            LOG("new command received");

            fdstream fs(client, -1);
            uint8_t respOK = OK;
            write(client, &respOK, 1);
            call_cmd(client);
            close(client);
        }
    }
private:
    int listenFd;
    string orisyncSock;
};

class Watchdog : public Thread
{
public:
    Watchdog() : Thread()
    {
    }
    void run() {
        time_t lastGC = time(NULL);
        string down("Down. Last connected ");
        char timeStr[26];

        while (!interruptionRequested()) {
          sleep(ORISYNC_WDINTERVAL);
          // check if hosts are still alive
          RWKey::sp key = hostsLock.readLock();
          for (auto &it : hosts) {
              if ((!it.second->isDown()) && (it.second->getTime() + HOST_TIMEOUT < time(NULL))) {
                  RWKey::sp hostLock = it.second->hostLock.writeLock();
                  it.second->setDown(true);
                  // Consider as host down
                  // TODO: should need a per host lock for update
                  int64_t time = it.second->getTime();
                  ctime_r(&time, timeStr);
                  string lasttime(timeStr);
                  lasttime.erase(lasttime.size() - 1);
                  it.second->setStatus((down + lasttime));
                  for (auto &repoID : myInfo.listRepos()) {
                      // Lock order: hostsLock->infoLock. We can collect and batch remove peer later to avoid grabbbing two locks here.
                      RWKey::sp key2 = myInfo.hostLock.readLock();
                      list<string> repos = myInfo.listRepos();
                      myInfo.removeRepoPeer(repoID, it.second->getHost());
                      key2.reset();

                  }
                  hostLock.reset();
              }
          }
          key.reset();


          if (lastGC + ORISYNC_GCINTERVAL > time(NULL)) {
            // time to do garbage collection
            //RWKey::sp key2 = infoLock.readLock();
            RWKey::sp key2 = myInfo.hostLock.readLock();
            list<string> repos = myInfo.listPaths();
            for (auto &it : repos) {
                RepoControl repo = RepoControl(it);
                try {
                    repo.open();
                } catch (SystemException &e) {
                    WARNING("Failed to open repository %s: %s", it.c_str(), e.what());
                    continue;
                }
                RWKey::sp repoKey = myInfo.getRepoLock(repo.getUUID())->writeLock();
                repo.gc(time(NULL) - ORISYNC_PURGETIME);
                repoKey.reset();
                repo.close();
            }
            key2.reset();
            lastGC = time(NULL);
          }
        }
        DLOG("Watchdog exited!");
    }
};

Listener *listener;
RepoMonitor *repoMonitor;
Syncer *syncer;
UdsServer *udsServer;
Watchdog *watchdog;

int
start_server()
{
    MSG("Starting OriSync");
    rc = OriSyncConf();
    listener = new Listener();
    repoMonitor = new RepoMonitor();
    syncer = new Syncer();
    udsServer = new UdsServer();
    watchdog = new Watchdog();

    myInfo = HostInfo(rc.getUUID(), rc.getCluster());
    // XXX: Update addresses periodically
    myInfo.setHost(OriStr_Join(OriNet_GetAddrs(), ','));
    MSG("User name: %s", OriNet_Username().c_str());
    myInfo.setUsername(OriNet_Username());

    listener->start();
    repoMonitor->start();
    syncer->start();
    watchdog->start();
    udsServer->start();


    /*
    struct event_base *base = event_base_new();
    struct evhttp *httpd = evhttp_new(base);
    evhttp_bind_socket(httpd, "0.0.0.0", 8051);

    evhttp_set_cb(httpd, "/", Httpd_getRoot, NULL);

    // Event loop
    event_base_dispatch(base);

    evhttp_free(httpd);
    event_base_free(base);
    */
    // XXX: Wait for worker threads
    unique_lock<mutex> lk(exitLock);
    exitCV.wait(lk);

    udsServer->interrupt();
    watchdog->interrupt();
    syncer->interrupt();
    repoMonitor->interrupt();
    listener->interrupt();

    // Wait for syncer to quit
    mrqCV.notify_one();
    syncer->wait();

    MSG("OriSync quits");

    return 0;
}

