#include "ori_fuse.h"

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
