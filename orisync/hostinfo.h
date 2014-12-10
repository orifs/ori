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

#ifndef __HOSTINFO_H__
#define __HOSTINFO_H__

class HostInfo {
public:
    HostInfo() {
    }
    HostInfo(const std::string &hostId, const std::string &cluster) {
        this->hostId = hostId;
        this->cluster = cluster;
        this->status = "OK";
        this->down = false;
    }
    ~HostInfo() {
    }
    void update(const KVSerializer &kv) {
        int numRepos;

        assert(hostId == kv.getStr("hostId"));
        assert(cluster == kv.getStr("cluster"));

        username = kv.getStr("username");
        host = kv.getStr("host");
        numRepos = kv.getU8("numRepos");

        repos.clear();
        allrepos.clear();

        for (int i = 0; i < numRepos; i++) {
            char prefix[32];
            std::map<std::string, RepoInfo>::iterator it;

            snprintf(prefix, sizeof(prefix), "repo.%d", i);

            RepoInfo info = RepoInfo();
            info.getKV(kv, prefix);

            repos[info.getRepoId()] = info;
            allrepos.push_back(info);
        }
    }
    std::string getBlob() const {
        KVSerializer kv;
        int i = 0;

        kv.putU64("time", (int64_t)time(NULL));
        kv.putStr("username", username);
        kv.putStr("host", host);
        kv.putStr("hostId", hostId);
        kv.putStr("cluster", cluster);
        kv.putU8("numRepos", repos.size());

        for (auto const &it : repos) {
            char prefix[32];

            snprintf(prefix, sizeof(prefix), "repo.%d", i);
	    i++;

            it.second.putKV(kv, prefix);
        }

        return kv.getBlob();
    }
    void setHost(const std::string &host) {
        this->host = host;
    }
    std::string getHost() {
        return host;
    }
    std::string getHostId() {
        return hostId;
    }
    bool hasRepo(const std::string &repoId) const {
        std::map<std::string, RepoInfo>::const_iterator it = repos.find(repoId);

        return it != repos.end();
    }
    RepoInfo getRepo(const std::string &repoId) {
        return repos[repoId];
    }
    void updateRepo(const std::string &repoId, const RepoInfo &info) {
        RepoInfo info_cpy = info;
        //do we want this condition? -Yinglei
        if (!hasRepo(repoId) || repos[repoId].getPath() == info_cpy.getPath()) {
            repos[repoId] = info;
        }
        for (auto it = std::begin(allrepos); it != std::end(allrepos); it++) {
            if ((info_cpy.getRepoId() == (*it).getRepoId()) &&
                (info_cpy.getPath() == (*it).getPath())) {
                LOG("replaced repo %s", (*it).getPath().c_str());
                it = allrepos.erase(it);
                allrepos.insert(it, info);
                return;
            }
        }
        //LOG("added repo %s", info_cpy.getPath().c_str());
        allrepos.push_back(info);
    }
    void removeRepo(const std::string &repoId) {
        std::map<std::string, RepoInfo>::iterator it = repos.find(repoId);

        repos.erase(it);

        for(auto iter = std::begin(allrepos); iter != std::end(allrepos);) {
            if((*iter).getRepoId() == repoId) {
                iter = allrepos.erase(iter);
            } else {
                iter++;
            }
        }
    } 
    void removePath(const std::string &path) {
        std::map<std::string, RepoInfo>::const_iterator it;
        for (auto &it : repos) {
            if (it.second.getPath() == path) {
                repos.erase(it.first);
                return;
            }
        }
    }
    std::list<std::string> listRepos() const {
        std::map<std::string, RepoInfo>::const_iterator it;
        std::list<std::string> val;

        for (auto const &it : repos) {
            val.push_back(it.first);
        }

        return val;
    }
    std::list<std::string> listPaths() {
        std::map<std::string, RepoInfo>::iterator it;
        std::list<std::string> val;

        for (auto &it : repos) {
            val.push_back(it.second.getPath());
        }

        return val;
    }
    std::list<RepoInfo> listRepoInfo() {
        std::map<std::string, RepoInfo>::const_iterator it;
        std::list<RepoInfo> val;

        for (auto const &it : repos) {
            val.push_back(it.second);
        }

        return val;
    }
    std::list<RepoInfo> listAllRepos() const {
        return allrepos;
    }
    void setPreferredIp(const std::string &ip) {
        preferredIp = ip;
    }
    std::string getPreferredIp() const {
        return preferredIp;
    }
    void setUsername(const std::string &uname) {
        username = uname;
    }
    std::string getUsername() const {
        return username;
    }
    void setStatus(const std::string &stat) {
        status = stat;
    }
    std::string getStatus() const {
        return status;
    }
    void setTime(const int64_t &time) {
        lasttime = time;
    }
    int64_t getTime() const {
        return lasttime;
    }
    void setDown(bool down) {
        this->down = down;
    }
    bool isDown() {
        return down;
    }
    void clearRepos() {
        repos.clear();
        allrepos.clear();
    }
    void insertRepoPeer(const std::string &repoID, const std::string &peer) {
        repos[repoID].insertPeer(peer);
    }
    void removeRepoPeer(const std::string &repoID, const std::string &peer) {
        repos[repoID].removePeer(peer);
    }

private:
    std::string preferredIp;
    std::string username;
    std::string host;
    std::string hostId;
    std::string cluster;
    std::string status;
    int64_t lasttime;
    bool down;
    std::map<std::string, RepoInfo> repos;
    std::list<RepoInfo> allrepos;
};

#endif /* __HOSTINFO_H__ */

