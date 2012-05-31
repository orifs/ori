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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "repo.h"

/*
 * Scan a directory.
 */
int
Scan_Traverse(const char *path,
              void *arg,
              int (*f)(void *arg, const char *))
{
    int status;
    DIR *dir;
    struct dirent *entry;
    char fullpath[PATH_MAX];

    if (path == NULL) {
        dir = opendir(".");
    } else {
        dir = opendir(path);
    }

    entry = readdir(dir);
    while ((entry = readdir(dir)) != NULL) {
        if (path != NULL) {
            strcpy(fullpath, path);
            strncat(fullpath, "/", PATH_MAX);
        } else {
            fullpath[0] = 0;
        }
        strncat(fullpath, entry->d_name, PATH_MAX);

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
int
Scan_RTraverse(const char *path,
               void *arg,
               int (*f)(void *arg, const char *))
{
    int status;
    DIR *dir;
    struct dirent *entry;
    char fullpath[PATH_MAX];

    if (path == NULL) {
        dir = opendir(".");
    } else {
        dir = opendir(path);
    }

    entry = readdir(dir);
    while ((entry = readdir(dir)) != NULL) {
        if (path != NULL) {
            strcpy(fullpath, path);
            strncat(fullpath, "/", PATH_MAX);
        } else {
            fullpath[0] = 0;
        }
        strncat(fullpath, entry->d_name, PATH_MAX);

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

        if (entry->d_type == DT_DIR) {
            status = Scan_RTraverse(fullpath, arg, f);
            if (status != 0) {
                closedir(dir);
                return status;
            }
        }
    }

    closedir(dir);
    return 0;
}

