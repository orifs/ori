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

#include <assert.h>

#include <unistd.h>

#include "openedfilemgr.h"

OpenedFileMgr::OpenedFileMgr()
    : numOpenHandles(0)
{
}

void OpenedFileMgr::openedFile(const std::string &tempFileName, int fd)
{
    openedFiles[tempFileName] += 1;
    fdToFilename[fd] = tempFileName;
    numOpenHandles += 1;
}

void OpenedFileMgr::closedFile(int fd)
{
    std::map<int, std::string>::iterator it = fdToFilename.find(fd);
    assert(it != fdToFilename.end());

    std::string tfn = (*it).second;
    fdToFilename.erase(it);
    closedFile(tfn);
}

void OpenedFileMgr::closedFile(const std::string &tempFileName)
{
    assert(openedFiles[tempFileName] > 0);
    assert(numOpenHandles > 0);

    openedFiles[tempFileName] -= 1;
    numOpenHandles -= 1;
}


void OpenedFileMgr::removeUnused()
{
    std::vector<std::string> toErase;

    for (std::map<std::string, uint32_t>::iterator it = openedFiles.begin();
            it != openedFiles.end();
            it++)
    {
        if ((*it).second == 0) {
            // Remove this file
            if (unlink((*it).first.c_str()) < 0) {
		// XXX: Too many errors printed!
		// perror("removedUnused unlink");
            }
            toErase.push_back((*it).first);
        }
    }

    for (size_t i = 0; i < toErase.size(); i++) {
        openedFiles.erase(toErase[i]);
    }
}

bool OpenedFileMgr::isOpen(const std::string &tempFileName) const
{
    std::map<std::string, uint32_t>::const_iterator it =
        openedFiles.find(tempFileName);
    if (it == openedFiles.end()) {
        return false;
    }
    return (*it).second > 0;
}

bool OpenedFileMgr::anyFilesOpen() const
{
    return numOpenHandles > 0;
}
