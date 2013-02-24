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

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <sstream>
#include <iostream>

#include <ori/debug.h>
#include <ori/oriutil.h>

#include "orisyncconf.h"

using namespace std;
#ifdef HAVE_CXXTR1
using namespace std::tr1;
#endif /* HAVE_CXXTR1 */

/********************************************************************
 *
 *
 * OriSyncConf
 *
 *
 ********************************************************************/

OriSyncConf::OriSyncConf()
    : name(""), key(""), repos(), rcFile("")
{
    string home = Util_GetHome();

    ASSERT(home != "");

    rcFile = home + "/.orisyncrc";
    if (Util_FileExists(rcFile)) {
        load();
    }
}

OriSyncConf::~OriSyncConf()
{
}

void
OriSyncConf::setCluster(const string &clusterName,
                        const string &clusterKey)
{
    name = clusterName;
    key = clusterKey;

    save();
}

string
OriSyncConf::getCluster() const
{
    return name;
}

string
OriSyncConf::getKey() const
{
    return key;
}

void
OriSyncConf::addRepo(const string &repoPath)
{
    repos.push_back(repoPath);

    save();
}

void
OriSyncConf::removeRepo(const string &repoPath)
{
    repos.remove(repoPath);

    save();
}

list<string>
OriSyncConf::getRepos() const
{
    return repos;
}

void
OriSyncConf::save() const
{
    if (rcFile != "") {
        string blob = getBlob();

        Util_WriteFile(blob.data(), blob.size(), rcFile);
    }
}

void
OriSyncConf::load()
{
    string blob = Util_ReadFile(rcFile);

    fromBlob(blob);
}

string
OriSyncConf::getBlob() const
{
    stringstream ss;
    string blob;
    
    blob = "# Cluster Configuration\n";
    blob += "cluster-name " + name + "\n";
    blob += "cluster-key " + key + "\n";

    blob += "\n# Repositories\n";
    for (list<string>::const_iterator it = repos.begin(); it != repos.end(); it++)
    {
        blob += "repo " + *it + "\n";
    }

    return blob;
}

void
OriSyncConf::fromBlob(const string &blob)
{
    string line;
    stringstream ss(blob);

    while (getline(ss, line, '\n')) {
        if (line.substr(0, 1) == "#" || line == "") {
            // comment or blank line
        } else if (line.substr(0, 13) == "cluster-name ") {
            name = line.substr(13);
        } else if (line.substr(0, 12) == "cluster-key ") {
            key = line.substr(12);
        } else if (line.substr(0, 5) == "repo ") {
            repos.push_back(line.substr(5));
        } else {
            printf("Invalid config file: %s\n", line.c_str());
            PANIC();
        }
    }
}

