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

#ifndef __REPO_H__
#define __REPO_H__

#include <stdint.h>

#include <string>
#include <set>

#include "tree.h"
#include "commit.h"

#define ORI_PATH_DIR "/.ori"
#define ORI_PATH_VERSION "/.ori/version"
#define ORI_PATH_UUID "/.ori/id"
#define ORI_PATH_DIRSTATE "/.ori/dirstate"
#define ORI_PATH_HEAD "/.ori/HEAD"
#define ORI_PATH_LOG "/.ori/ori.log"
#define ORI_PATH_TMP "/.ori/tmp/"
#define ORI_PATH_OBJS "/.ori/objs/"

#define EMPTY_COMMIT "0000000000000000000000000000000000000000000000000000000000000000"

class Repo
{
public:
    Repo(const std::string &root = "");
    ~Repo();
    bool open();
    void close();
    void save();
    // Object Operations
    std::string addFile(const std::string &path);
    std::string addBlob(const std::string &blob);
    std::string addTree(/* const */ Tree &tree);
    std::string addCommit(/* const */ Commit &commit);
    char *getObject(const std::string &objId);
    size_t getObjectLength(const std::string &objId);
    size_t sendObject(const char *objId);
    bool copyObject(const std::string &objId, const std::string &path);
    std::set<std::string> getObjects();
    Commit getCommit(const std::string &commitId);
    // Working Directory Operations
    std::string getHead();
    void updateHead(const std::string &commitId);
    // General Operations
    std::string getUUID();
    std::string getVersion();
    // Static operations
    static std::string getRootPath();
    static std::string getLogPath();
    static std::string getTmpFile();
private:
    // Helper Functions
    void createObjDirs(const std::string &objId);
    std::string objIdToPath(const std::string &objId);
    // Variables
    std::string rootPath;
    std::string id;
    std::string version;
};

extern Repo repository;

#endif /* __REPO_H__ */

