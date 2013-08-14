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

#ifdef HAVE_EXECINFO
#include <execinfo.h>
#endif /* HAVE_EXECINFO */

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

/*
 * Check if a file exists.
 */
bool
Util_FileExists(const string &path)
{
    struct stat sb;

    if (stat(path.c_str(), &sb) == 0)
        return true;

    return false;
}

int
Util_MkDir(const string &path)
{
    if (mkdir(path.c_str(), 0755) < 0) {
        perror("mkdir failed");
        return -errno;
    }

    return 0;
}

int
Util_RmDir(const string &path)
{
    if (rmdir(path.c_str()) < 0) {
        return -errno;
    }

    return 0;
}

/*
 * Check if a file is a directory.
 */
bool
Util_IsDirectory(const string &path)
{
    struct stat sb;

    if (stat(path.c_str(), &sb) < 0) {
        perror("stat file does not exist");
        return false;
    }

    if (sb.st_mode & S_IFDIR)
        return true;
    else
        return false;
}

/*
 * Get the file size.
 */
size_t Util_FileSize(const std::string &path)
{
    struct stat sb;

    if (stat(path.c_str(), &sb) < 0) {
        perror("stat file does not exist");
        return -errno;
    }

    return sb.st_size;
}

/*
 * Return the full path given a relative path.
 * XXX: According to FreeBSD man pages this may be broken on Solaris.
 */
string
Util_RealPath(const string &path)
{
    char *tmp;
    string rval = "";

    tmp = realpath(path.c_str(), NULL);
    if (tmp) {
        rval = tmp;
        free(tmp);
    }

    return rval;
}

/*
 * Read a file containing a text string into memory. There may not be any null 
 * characters in the file.
 */
std::string
Util_ReadFile(const string &path)
{
    diskstream ds(path);
    return ds.readAll();

    /*FILE *f;
    char *buf;
    size_t len;

    f = fopen(path.c_str(), "rb");
    if (f == NULL) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = new char[len + 1];

    fread(buf, 1, len, f);
    fclose(f);

    // Just for simplicity we add a null charecter at the end to allow us to 
    // read and write strings easily.
    buf[len] = '\0';

    if (flen != NULL)
        *flen = len;

    return buf;*/
}

/*
 * Write an in memory blob to a file.
 */
bool
Util_WriteFile(const std::string &blob, const string &path)
{
    return Util_WriteFile(blob.data(), blob.length(), path);
}

/*
 * Write an in memory blob to a file.
 */
bool
Util_WriteFile(const char *blob, size_t len, const string &path)
{
    FILE *f;
    size_t bytesWritten;

    f = fopen(path.c_str(), "w+");
    if (f == NULL) {
        return false;
    }

    bytesWritten = fwrite(blob, len, 1, f);
    fclose(f);

    return (bytesWritten == 1);
}

/*
 * Append an in memory blob to a file.
 */
bool
Util_AppendFile(const std::string &blob, const string &path)
{
    return Util_WriteFile(blob.data(), blob.length(), path);
}

/*
 * Append an in memory blob to a file.
 */
bool
Util_AppendFile(const char *blob, size_t len, const string &path)
{
    FILE *f;
    size_t bytesWritten;

    f = fopen(path.c_str(), "a+");
    if (f == NULL) {
        return false;
    }

    bytesWritten = fwrite(blob, len, 1, f);
    fclose(f);

    return (bytesWritten == 1);
}

/*
 * Copy a file.
 */
int
Util_CopyFile(const string &origPath, const string &newPath)
{
    int srcFd, dstFd;
    char buf[COPYFILE_BUFSZ];
    struct stat sb;
    int64_t bytesLeft;
    int64_t bytesRead, bytesWritten;

    srcFd = open(origPath.c_str(), O_RDONLY);
    if (srcFd < 0)
        return -errno;

    dstFd = open(newPath.c_str(), O_WRONLY | O_CREAT,
                 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (dstFd < 0) {
        close(srcFd);
        return -errno;
    }

    if (fstat(srcFd, &sb) < 0) {
        close(srcFd);
        unlink(newPath.c_str());
        close(dstFd);
        return -errno;
    }

    bytesLeft = sb.st_size;
    while(bytesLeft > 0) {
        bytesRead = read(srcFd, buf, MIN(bytesLeft, COPYFILE_BUFSZ));
        if (bytesRead < 0) {
            if (errno == EINTR)
                continue;
            goto error;
        }

retryWrite:
        bytesWritten = write(dstFd, buf, bytesRead);
        if (bytesWritten < 0) {
            if (errno == EINTR)
                goto retryWrite;
            goto error;
        }

        // XXX: Need to handle this case!
        ASSERT(bytesRead == bytesWritten);

        bytesLeft -= bytesRead;
    }

    close(srcFd);
    close(dstFd);
    return sb.st_size;

error:
    close(srcFd);
    unlink(newPath.c_str());
    close(dstFd);
    return -errno;
}

/*
 * Safely move a file possibly between file systems.
 */
int
Util_MoveFile(const string &origPath, const string &newPath)
{
    int status = 0;

    if (rename(origPath.c_str(), newPath.c_str()) < 0)
        status = -errno;

    // If the file is on seperate file systems copy it and delete the original.
    if (errno == EXDEV) {
        status = Util_CopyFile(origPath, newPath);
        if (status < 0)
            return status;

        if (unlink(origPath.c_str()) < 0) {
            status = -errno;
            ASSERT(false);
        }
    }

    return status;
}

/*
 * Delete a file.
 */
int
Util_DeleteFile(const std::string &path)
{
    if (unlink(path.c_str()) < 0)
        return -errno;

    return 0;
}

/*
 * Rename a file.
 */
int
Util_RenameFile(const std::string &from, const std::string &to)
{
    if (rename(from.c_str(), to.c_str()) < 0)
        return -errno;

    return 0;
}

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

std::string
StrUtil_Basename(const std::string &path)
{
    size_t ix = path.rfind('/');
    if (ix == std::string::npos) {
        return path;
    }
    return path.substr(ix+1);
}

std::string
StrUtil_Dirname(const std::string &path)
{
    size_t ix = path.rfind('/');
    if (ix == std::string::npos) {
        return path;
    }
    return path.substr(0, ix);
}

// XXX: Debug Only

#define TESTFILE_SIZE (HASHFILE_BUFSZ * 3 / 2)

int
OriUtil_selfTest(void)
{
    int status;
    char *buf = new char[TESTFILE_SIZE + 1];
    ObjectHash origHash;
    ObjectHash newHash;

    cout << "Testing OriUtil ..." << endl;

    for (int i = 0; i < TESTFILE_SIZE; i++) {
        buf[i] = '0' + (i % 10);
    }
    buf[TESTFILE_SIZE] = '\0';
    origHash = OriCrypt_HashString(buf);

    Util_WriteFile(buf, TESTFILE_SIZE, "test.orig");

    status = Util_CopyFile("test.orig", "test.a");
    if (status < 0) {
        printf("Util_CopyFile: %s\n", strerror(-status));
        ASSERT(false);
    }
    status = Util_CopyFile("test.orig", "test.b");
    if (status < 0) {
        printf("Util_CopyFile: %s\n", strerror(-status));
        ASSERT(false);
    }
    status = Util_MoveFile("test.b", "test.c");
    if (status < 0) {
        printf("Util_CopyFile: %s\n", strerror(-status));
        ASSERT(false);
    }
    std::string fbuf = Util_ReadFile("test.c");
    //ASSERT(fbuf);
    newHash = OriCrypt_HashString(fbuf);
    // XXX: Check that 'test.b' does not exist

    if (newHash != origHash) {
        printf("Hash mismatch!\n");
        ASSERT(false);
    }

    newHash = OriCrypt_HashFile("test.a");

    if (newHash != origHash) {
        printf("Hash mismatch!\n");
        ASSERT(false);
    }

    vector<string> tv;
    tv = Util_PathToVector("/abc/def");
    ASSERT(tv.size() == 2);
    ASSERT(tv[0] == "abc");
    ASSERT(tv[1] == "def");

    // Tests for stream classes
    int test_fd = open("test.a", O_RDWR | O_CREAT, 0644);
    write(test_fd, "hello, world!", 13);
    fsync(test_fd);
    bytestream::ap bs(new fdstream(test_fd, 1, 3));
    uint8_t b;
    bs->read(&b, 1);
    ASSERT(b == 'e');

    ASSERT(Util_DeleteFile("test.a") == 0);
    ASSERT(Util_FileExists("test.a") == false);
    ASSERT(Util_DeleteFile("test.c") == 0);
    ASSERT(Util_FileExists("test.c") == false);
    ASSERT(Util_DeleteFile("test.orig") == 0);
    ASSERT(Util_FileExists("test.orig") == false);

    // Tests for string utilities
    std::string path("hello.txt");
    ASSERT(StrUtil_Basename(path) == "hello.txt");
    ASSERT(Util_PathToVector(path).size() == 1);
    path = "a/hello.txt";
    ASSERT(StrUtil_Basename(path) == "hello.txt");
    ASSERT(Util_PathToVector(path).size() == 2);
    path = "/hello.txt";
    ASSERT(StrUtil_Basename(path) == "hello.txt");
    ASSERT(Util_PathToVector(path).size() == 1);
    path = "/a/b/c/hello.txt";
    ASSERT(StrUtil_Basename(path) == "hello.txt");
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

