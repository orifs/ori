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

#ifndef __SSHREPO_H__
#define __SSHREPO_H__

#include "repo.h"
#include "sshclient.h"

class SshRepo : public Repo
{
public:
    SshRepo(SshClient *client);
    ~SshRepo();

    std::string getHead();

    BaseObject *getObject(const std::string &id);
    bool hasObject(const std::string &id);
    std::set<std::string> listObjects();
    int addObjectRaw(const Object::ObjectInfo &info,
            const std::string &raw_data);
    //Object addObjectRaw(Object::ObjectInfo info, bytestream *bs);
    //std::string addObject(Object::ObjectInfo info, const std::string &data);

private:
    SshClient *client;
};

#endif /* __SSHREPO_H__ */
