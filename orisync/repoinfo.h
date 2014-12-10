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

#ifndef __REPOINFO_H__
#define __REPOINFO_H__

#include <set>

class RepoInfo {
public:
    RepoInfo() {
    }
    /*
    RepoInfo(const std::string &repoId, const std::string &path) {
        this->repoId = repoId;
        this->path = path;
        hasRemote = false;
    }
    */
    RepoInfo(const std::string &repoId, const std::string &path, bool mounted) {
        this->repoId = repoId;
        this->path = path;
        this->mounted = mounted;
        remote = false;
    }
    ~RepoInfo() {
    }
    std::string getRepoId() {
        return repoId;
    }
    void updateHead(const std::string &head) {
        this->head = head;
    }
    std::string getHead() {
        return head;
    }
    std::string getPath() {
        return path;
    }
    void getKV(const KVSerializer &kv, const std::string &prefix) {
        repoId = kv.getStr(prefix + ".id");
        head = kv.getStr(prefix + ".head");
        path = kv.getStr(prefix + ".path");
        mounted = kv.getU8(prefix + ".mount");
    }
    void putKV(KVSerializer &kv, const std::string &prefix) const {
        kv.putStr(prefix + ".id", repoId);
        kv.putStr(prefix + ".head", head);
        kv.putStr(prefix + ".path", path);
        kv.putU8(prefix + ".mount", mounted);
    }
    bool isMounted() {
        return mounted;
    }
    void setMounted(bool mounted) {
        this->mounted = mounted;
    }
    void insertPeer(const std::string &peer) {
        peers.insert(peer);
        remote = true;
    }
    void removePeer(const std::string &peer) {
        peers.erase(peer);
        if (peers.empty())
            remote = false;
    }
    bool hasRemote() {
        return remote;
    }
    std::string listPeers() {
        std::string rval = "";
        if (peers.empty())
            return rval;

        for (std::string p : peers) {
            if (rval == "") {
                rval = p;
            } else {
                rval += " ";
                rval += p;
            }
        }
        return rval;
    }
    bool hasPeer(std::string p) {
        return (peers.find(p) != peers.end());
    }
private:
    std::string repoId;
    std::string head;
    std::string path;
    uint8_t mounted;
    std::set<std::string> peers;
    bool remote;
};

#endif /* __REPOINFO_H__ */

