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
    }
    ~HostInfo() {
    }
    void update(const KVSerializer &kv) {
        int numRepos;

        assert(hostId == kv.getStr("hostId"));
        assert(cluster == kv.getStr("cluster"));

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

        kv.putU64("time", (uint64_t)time(NULL));
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
        repos[repoId] = info;
        for (auto it = std::begin(allrepos); it != std::end(allrepos); it++) {
            if ((info_cpy.getRepoId() == (*it).getRepoId()) &&
                (info_cpy.getPath() == (*it).getPath())) {
                LOG("replaced repo %s", (*it).getPath().c_str());
                it = allrepos.erase(it);
                allrepos.insert(it, info);
                return;
            }
        }
        LOG("added repo %s", info_cpy.getPath().c_str());
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
    std::list<RepoInfo> listRepos() const {
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
private:
    std::string preferredIp;
    std::string host;
    std::string hostId;
    std::string cluster;
    std::map<std::string, RepoInfo> repos;
    std::list<RepoInfo> allrepos;
};

#endif /* __HOSTINFO_H__ */

