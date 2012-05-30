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

#include <string>
#include <map>
#include <set>

using namespace std;

#define ORI_PATH_DIR "/.ori"
#define ORI_PATH_VERSION "/.ori/version"
#define ORI_PATH_UUID "/.ori/id"
#define ORI_PATH_DIRSTATE "/.ori/dirstate"
#define ORI_PATH_LOG "/.ori/ori.log"
#define ORI_PATH_TMP "/.ori/tmp/"
#define ORI_PATH_OBJS "/.ori/objs/"

class Tree
{
public:
    Tree();
    ~Tree();
    void addObject(const char *path, const char *objid);
    const std::string getBlob();
private:
};

class Commit
{
public:
    Commit();
    ~Commit();
    void setParents(std::string p1, std::string p2 = "");
    std::set<std::string> getParents();
    void setMessage(std::string msg);
    std::string getMessage() const;
    void setTree(std::string &tree);
    std::string getTree() const;
    const std::string getBlob();
private:
};

class Repo
{
public:
    Repo();
    ~Repo();
    bool open();
    void close();
    void save();
    // Object Operations
    string addFile(const string &path);
    string addBlob(const string &blob);
    string addTree(/* const */ Tree &tree);
    string addCommit(const string &commit);
    char *getObject(const string &objId);
    size_t getObjectLength(const char *objId);
    size_t sendObject(const char *objId);
    bool copyObject(const char *objId, const char *path);
    std::set<std::string> getObjects();
    // General Operations
    std::string getUUID();
    std::string getVersion();
    static std::string getRootPath();
    static std::string getLogPath();
    static std::string getTmpFile();
private:
    string id;
    string version;
};

extern Repo repository;

#endif /* __REPO_H__ */

