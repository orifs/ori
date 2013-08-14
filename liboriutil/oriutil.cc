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

#define __STDC_LIMIT_MACROS

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>

#if (defined(__FreeBSD__) && !defined(__APPLE__)) || defined(__NetBSD__)
#include <uuid.h>
#endif /* __FreeBSD__ */

#if defined(__APPLE__) || defined(__linux__)
#include <uuid/uuid.h>
#endif

#include "tuneables.h"

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/oricrypt.h>
#include <oriutil/stream.h>

using namespace std;

vector<string>
Util_PathToVector(const string &path)
{
    size_t pos = 0;
    vector<string> rval;

    if (path[0] == '/')
        pos = 1;

    while (pos < path.length()) {
        size_t end = path.find('/', pos);
        if (end == path.npos) {
            rval.push_back(path.substr(pos));
            break;
        }

        rval.push_back(path.substr(pos, end - pos));
        pos = end + 1;
    }

    return rval;
}

string
Util_GetFullname()
{
    struct passwd *pw = getpwuid(getuid());

    // XXX: Error handling
    ASSERT(pw != NULL);

    if (pw->pw_gecos != NULL)
        return string(pw->pw_gecos);
    else
        return "";
}

string
Util_GetHome()
{
    char *path;

    path = getenv("HOME");
    if (path == NULL)
        return "";

    return path;
}

int Util_SetBlocking(int fd, bool block) {
    int old_flags = fcntl(fd, F_GETFL);
    if (old_flags < 0) {
        perror("fcntl");
        return old_flags;
    }

    if (block) {
        if (old_flags & O_NONBLOCK) {
            fcntl(fd, F_SETFL, (old_flags & ~O_NONBLOCK));
        }
    }
    else {
        if (!(old_flags & O_NONBLOCK)) {
            fcntl(fd, F_SETFL, old_flags | O_NONBLOCK);
        }
    }

    return 0;
}

/*
 * Generate a UUID
 */
std::string
Util_NewUUID()
{
#if (defined(__FreeBSD__) && !defined(__APPLE__)) || defined(__NetBSD__)
    uint32_t status;
    uuid_t id;
    char *uuidBuf;

    uuid_create(&id, &status);
    if (status != uuid_s_ok) {
        printf("Failed to construct UUID!\n");
        return "";
    }
    uuid_to_string(&id, &uuidBuf, &status);
    if (status != uuid_s_ok) {
        printf("Failed to print UUID!\n");
        return "";
    }
    std::string rval(uuidBuf);
    free(uuidBuf);
    return rval;
#endif /* __FreeBSD__ */

#if defined(__APPLE__) || defined(__linux__)
    uuid_t id;
    uuid_generate(id);
    char id_buf[128];
    uuid_unparse(id, id_buf);
    
    std::string rval(id_buf);
    return rval;
#endif /* __APPLE__ || __linux__ */
}

bool
Util_IsPathRemote(const string &path) {
    size_t pos = path.find_first_of(':');
    if (pos == 0) return false;
    if (pos == path.npos) return false;
    return true;
}

// XXX: Debug Only

int
OriUtil_selfTest(void)
{
    cout << "Testing OriUtil ..." << endl;

    // Tests for string utilities
    std::string path("hello.txt");
    ASSERT(Util_PathToVector(path).size() == 1);
    path = "a/hello.txt";
    ASSERT(Util_PathToVector(path).size() == 2);
    path = "/hello.txt";
    ASSERT(Util_PathToVector(path).size() == 1);
    path = "/a/b/c/hello.txt";
    ASSERT(Util_PathToVector(path).size() == 4);

    path = "/b/b.txt/file";
    std::vector<string> pv = Util_PathToVector(path);
    ASSERT(pv.size() == 3);
    ASSERT(pv[0] == "b");
    ASSERT(pv[1] == "b.txt");
    ASSERT(pv[2] == "file");

    // Test for serialization
    std::string testStr;
    uint64_t nums[] = {129, 76, 13892, 45, 16777217, 14877928, UINT32_MAX * 4,
        24879878};
    strwstream out_stream;
    out_stream.writeUInt8(nums[0]);
    out_stream.writeInt8(nums[1]);
    out_stream.writePStr("hello, world!");
    out_stream.writeLPStr("hello, world!");
    out_stream.writeUInt16(nums[2]);
    out_stream.writeInt16(nums[3]);
    out_stream.writeUInt32(nums[4]);
    out_stream.writeInt32(nums[5]);
    out_stream.writeUInt64(nums[6]);
    out_stream.writeInt64(nums[7]);

    strstream in_stream(out_stream.str());
    ASSERT(in_stream.readUInt8() == nums[0]);
    ASSERT(in_stream.readInt8() == (int8_t)nums[1]);
    in_stream.readPStr(testStr);
    ASSERT(testStr == "hello, world!");
    in_stream.readLPStr(testStr);
    ASSERT(testStr == "hello, world!");
    ASSERT(in_stream.readUInt16() == nums[2]);
    ASSERT(in_stream.readInt16() == (int16_t)nums[3]);
    ASSERT(in_stream.readUInt32() == nums[4]);
    ASSERT(in_stream.readInt32() == (int32_t)nums[5]);
    ASSERT(in_stream.readUInt64() == nums[6]);
    ASSERT(in_stream.readInt64() == (int64_t)nums[7]);

    return 0;
}

