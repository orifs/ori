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

#ifndef __REPOCONTROL_H__
#define __REPOCONTROL_H__

#include <map>

#include <ori/repo.h>
#include <ori/localrepo.h>
#include <ori/udsclient.h>
#include <ori/udsrepo.h>

class RepoControl {
public:
    RepoControl(const std::string &path);
    ~RepoControl();
    void open();
    void close();
    std::string getPath();
    std::string getUUID();
    std::string getHead();
    bool hasCommit(const std::string &objId);
    std::string pull(const std::string &host, const std::string &path);
    std::string push(const std::string &host, const std::string &path);
    int snapshot();
    void gc(time_t time);
private:
    std::string path;
    std::string uuid;
    UDSClient *udsClient;
    UDSRepo *udsRepo;
    LocalRepo *localRepo;
    int checkout(ObjectHash newHead);
};

#endif /* __REPOCONTROL_H__ */

