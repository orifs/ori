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

#ifndef __SCAN_H__
#define __SCAN_H__

#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

/*
 * Scan a directory.
 */
template <typename T>
int
DirIterate(const std::string &path,
           T arg,
           int (*f)(T, const std::string&))
{
    int status;
    DIR *dir;
    struct dirent *entry;
    std::string fullpath;

    if (path == "") {
        dir = opendir(".");
    } else {
        dir = opendir(path.c_str());
    }

    while ((entry = readdir(dir)) != NULL) {
        if (path != "") {
            fullpath = path + "/";
        } else {
            fullpath = "";
        }
        fullpath += entry->d_name;

        if (strcmp(entry->d_name, ".") == 0)
            continue;
        if (strcmp(entry->d_name, "..") == 0)
            continue;

        // '.ori' should never be scanned
        if (strcmp(entry->d_name, ".ori") == 0)
            continue;

        if (f != NULL) {
            status = f(arg, fullpath);
            if (status != 0) {
                closedir(dir);
                return status;
            }
        }
    }

    closedir(dir);
    return 0;
}

/*
 * Recursive traverse of a directory tree.
 */
template <typename T>
int
DirTraverse(const std::string &path,
           T arg,
           int (*f)(T, const std::string&))
{
    int status;
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    std::string fullpath;

    if (path == "") {
        dir = opendir(".");
    } else {
        dir = opendir(path.c_str());
    }

    if (dir == NULL) {
        perror("Scan_RTraverse opendir");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (path != "") {
            fullpath = path + "/";
        } else {
            fullpath = "";
        }
        fullpath += entry->d_name;

        if (strcmp(entry->d_name, ".") == 0)
            continue;
        if (strcmp(entry->d_name, "..") == 0)
            continue;

        // '.ori' should never be scanned
        if (strcmp(entry->d_name, ".ori") == 0)
            continue;

        if (f != NULL) {
            status = f(arg, fullpath);
            if (status != 0) {
                closedir(dir);
                return status;
            }
        }

        // This check avoids symbol links to directories.
        if (entry->d_type == DT_DIR) {
            status = DirTraverse(fullpath, arg, f);
            if (status != 0) {
                //closedir(dir);
                //return status;
                fprintf(stderr, "Couldn't scan directory %s\n", fullpath.c_str());
                continue;
            }
        }
    }

    closedir(dir);
    return 0;
}


#endif /* __SCAN_H__ */

