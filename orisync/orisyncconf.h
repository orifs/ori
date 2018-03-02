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

#ifndef __ORISYNCCONF_H__
#define __ORISYNCCONF_H__

#include <stdint.h>

#include <string>
#include <list>

class OriSyncConf
{
public:
    OriSyncConf();
    ~OriSyncConf();
    void setCluster(const std::string &clusterName,
                    const std::string &clusterKey);
    void setUUID(const std::string &uuid);
    std::string getCluster() const;
    std::string getKey() const;
    std::string getUUID() const;
    void addRepo(const std::string &repoPath, bool saveToDisk);
    void removeRepo(const std::string &repoPath, bool saveToDisk);
    std::list<std::string> getRepos() const;
    void addHost(const std::string &host);
    void removeHost(const std::string &host);
    std::list<std::string> getHosts() const;
private:
    void load();
    void save() const;
    std::string getBlob() const;
    void fromBlob(const std::string &blob);
    // Persistent state
    std::string name;
    std::string key;
    std::string machineid;
    std::list<std::string> repos;
    std::list<std::string> hosts;
    // Volatile State
    std::string rcFile;
};

#endif /* __ORISYNCCONF_H__ */

