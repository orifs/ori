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
#include <oriutil/orifile.h>
#include <oriutil/oricrypt.h>
#include <oriutil/stream.h>

using namespace std;

/*
 * Check if a file exists.
 */
bool
OriFile_Exists(const string &path)
{
    struct stat sb;

    if (stat(path.c_str(), &sb) == 0)
        return true;

    return false;
}

int
OriFile_MkDir(const string &path)
{
    if (mkdir(path.c_str(), 0755) < 0) {
        perror("mkdir failed");
        return -errno;
    }

    return 0;
}

int
OriFile_RmDir(const string &path)
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
OriFile_IsDirectory(const string &path)
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
size_t
OriFile_GetSize(const std::string &path)
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
OriFile_RealPath(const string &path)
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
OriFile_ReadFile(const string &path)
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
OriFile_WriteFile(const std::string &blob, const string &path)
{
    return OriFile_WriteFile(blob.data(), blob.length(), path);
}

/*
 * Write an in memory blob to a file.
 */
bool
OriFile_WriteFile(const char *blob, size_t len, const string &path)
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
OriFile_Append(const std::string &blob, const string &path)
{
    return OriFile_Append(blob.data(), blob.length(), path);
}

/*
 * Append an in memory blob to a file.
 */
bool
OriFile_Append(const char *blob, size_t len, const string &path)
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
OriFile_Copy(const string &origPath, const string &newPath)
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
OriFile_Move(const string &origPath, const string &newPath)
{
    int status = 0;

    if (rename(origPath.c_str(), newPath.c_str()) < 0)
        status = -errno;

    // If the file is on seperate file systems copy it and delete the original.
    if (errno == EXDEV) {
        status = OriFile_Copy(origPath, newPath);
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
OriFile_Delete(const std::string &path)
{
    if (unlink(path.c_str()) < 0)
        return -errno;

    return 0;
}

/*
 * Rename a file.
 */
int
OriFile_Rename(const std::string &from, const std::string &to)
{
    if (rename(from.c_str(), to.c_str()) < 0)
        return -errno;

    return 0;
}

std::string
OriFile_Basename(const std::string &path)
{
    size_t ix = path.rfind('/');
    if (ix == std::string::npos) {
        return path;
    }
    return path.substr(ix+1);
}

std::string
OriFile_Dirname(const std::string &path)
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
OriFile_selfTest(void)
{
    int status;
    char *buf = new char[TESTFILE_SIZE + 1];
    ObjectHash origHash;
    ObjectHash newHash;

    cout << "Testing OriFile ..." << endl;

    for (int i = 0; i < TESTFILE_SIZE; i++) {
        buf[i] = '0' + (i % 10);
    }
    buf[TESTFILE_SIZE] = '\0';
    origHash = OriCrypt_HashString(buf);

    OriFile_WriteFile(buf, TESTFILE_SIZE, "test.orig");

    status = OriFile_Copy("test.orig", "test.a");
    if (status < 0) {
        printf("OriFile_Copy: %s\n", strerror(-status));
        ASSERT(false);
    }
    status = OriFile_Copy("test.orig", "test.b");
    if (status < 0) {
        printf("OriFile_Copy: %s\n", strerror(-status));
        ASSERT(false);
    }
    status = OriFile_Move("test.b", "test.c");
    if (status < 0) {
        printf("OriFile_Move: %s\n", strerror(-status));
        ASSERT(false);
    }
    std::string fbuf = OriFile_ReadFile("test.c");
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

    // Tests for stream classes
    int test_fd = open("test.a", O_RDWR | O_CREAT, 0644);
    write(test_fd, "hello, world!", 13);
    fsync(test_fd);
    bytestream::ap bs(new fdstream(test_fd, 1, 3));
    uint8_t b;
    bs->read(&b, 1);
    ASSERT(b == 'e');

    ASSERT(OriFile_Delete("test.a") == 0);
    ASSERT(OriFile_Exists("test.a") == false);
    ASSERT(OriFile_Delete("test.c") == 0);
    ASSERT(OriFile_Exists("test.c") == false);
    ASSERT(OriFile_Delete("test.orig") == 0);
    ASSERT(OriFile_Exists("test.orig") == false);

    // Tests for string utilities
    std::string path("hello.txt");
    ASSERT(OriFile_Basename(path) == "hello.txt");
    path = "a/hello.txt";
    ASSERT(OriFile_Basename(path) == "hello.txt");
    path = "/hello.txt";
    ASSERT(OriFile_Basename(path) == "hello.txt");
    path = "/a/b/c/hello.txt";
    ASSERT(OriFile_Basename(path) == "hello.txt");

    return 0;
}

