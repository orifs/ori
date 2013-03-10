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

#ifndef __PEER_H__
#define __PEER_H__

#include <stdint.h>
#include <time.h>

#include <utility>
#include <string>
#include <boost/tr1/memory.hpp>

class Repo;
class HttpClient;
class SshClient;

/*
 * Peer class manages connections to other hosts and persists information to 
 * disk. Eventually the last synced commitId should be saved as an 
 * optimization.
 */

class Peer
{
public:
    Peer();
    explicit Peer(const std::string &peerFile);
    ~Peer();
    void setUrl(const std::string &url);
    std::string getUrl() const;
    void setRepoId(const std::string &uuid);
    std::string getRepoId() const;
    void setInstaClone(bool ic);
    bool isInstaCloning() const;
    Repo *getRepo();
private:
    void save() const;
    std::string getBlob() const;
    void fromBlob(const std::string &blob);
    // Persistent state
    bool instaCloning;
    std::string url;
    std::string repoId;
    // Volatile State
    std::string peerFile;
    std::tr1::shared_ptr<Repo> cachedRepo;
    std::tr1::shared_ptr<HttpClient> hc;
    std::tr1::shared_ptr<SshClient> sc;
};

#endif /* __PEER_H__ */

