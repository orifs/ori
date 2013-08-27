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

        for (int i = 0; i < numRepos; i++) {
            char prefix[32];
            std::map<std::string, RepoInfo>::iterator it;

            snprintf(prefix, sizeof(prefix), "repo.%d", i);

            RepoInfo info = RepoInfo();
            info.getKV(kv, prefix);

            repos[info.getRepoId()] = info;
        }
    }
    std::string getBlob() const {
        KVSerializer kv;
        int i;
        std::map<std::string, RepoInfo>::const_iterator it;

        kv.putU64("time", (uint64_t)time(NULL));
        kv.putStr("host", host);
        kv.putStr("hostId", hostId);
        kv.putStr("cluster", cluster);
        kv.putU8("numRepos", repos.size());

        for (i = 0, it = repos.begin(); it != repos.end(); i++, it++) {
            char prefix[32];

            snprintf(prefix, sizeof(prefix), "repo.%d", i);

            it->second.putKV(kv, prefix);
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
        repos[repoId] = info;
    }
    void removeRepo(const std::string &repoId) {
        std::map<std::string, RepoInfo>::iterator it = repos.find(repoId);

        repos.erase(it);
    }
    std::list<std::string> listRepos() const {
        std::map<std::string, RepoInfo>::const_iterator it;
        std::list<std::string> val;

        for (it = repos.begin(); it != repos.end(); it++) {
            val.push_back(it->first);
        }

        return val;
    }
private:
    std::string host;
    std::string hostId;
    std::string cluster;
    std::map<std::string, RepoInfo> repos;
};

#endif /* __HOSTINFO_H__ */

