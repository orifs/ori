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

class RepoInfo {
public:
    RepoInfo() {
    }
    ~RepoInfo() {
    }
    std::string getRepoId() {
        return repoId;
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
    }
    void putKV(KVSerializer &kv, const std::string &prefix) const {
        kv.putStr(prefix + ".id", repoId);
        kv.putStr(prefix + ".head", head);
        kv.putStr(prefix + ".path", path);
    }
private:
    std::string repoId;
    std::string head;
    std::string path;
};

#endif /* __REPOINFO_H__ */

