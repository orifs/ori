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

#ifndef __OPENEDFILEMGR_H__
#define __OPENEDFILEMGR_H__

#include <stdbool.h>
#include <stdint.h>

#include <string>
#include <map>

#include <ori/rwlock.h>

class OpenedFileMgr
{
public:
    OpenedFileMgr();

    void openedFile(const std::string &tempFileName, int fd);
    void closedFile(int fd);
    void closedFile(const std::string &tempFileName);

    void removeUnused();
    bool isOpen(const std::string &tempFileName) const;
    bool anyFilesOpen() const;

    /// Clients responsible for their own locking
    RWLock lock_tempfiles;

private:
    uint64_t numOpenHandles;
    // map from temp filename to number of open handles
    std::map<std::string, uint32_t> openedFiles;
    std::map<int, std::string> fdToFilename;
};

#endif /* __OPENEDFILEMGR_H__ */

