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

#ifndef __COMMIT_H__
#define __COMMIT_H__

#include <stdint.h>
#include <time.h>

#include <utility>
#include <string>

class Commit
{
public:
    Commit();
    Commit(const std::string &msg, const std::string &tree, const std::string &user);
    ~Commit();
    void setParents(std::string p1, std::string p2 = "");
    std::pair<std::string, std::string> getParents() const;
    void setMessage(const std::string &msg);
    std::string getMessage() const;
    void setTree(const std::string &tree);
    std::string getTree() const;
    void setUser(const std::string &user);
    std::string getUser() const;
    void setSnapshot(const std::string &snapshot);
    std::string getSnapshot() const;
    void setTime(time_t t);
    time_t getTime() const;
    void setGraft(const std::string &repo,
		  const std::string &path,
		  const std::string &commidId);
    std::pair<std::string, std::string> getGraftRepo() const;
    std::string getGraftCommit() const;
    std::string getBlob() const;
    void fromBlob(const std::string &blob);

    std::string hash() const; // TODO: cache this
private:
    std::pair<std::string, std::string> parents;
    std::string message;
    std::string treeObjId;
    std::string user;
    std::string snapshotName;
    time_t date;
    std::string graftRepo;
    std::string graftPath;
    std::string graftCommitId;
};

#endif /* __COMMIT_H__ */

