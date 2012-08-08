#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sstream>
#include <stdexcept>

#include "packfile.h"
#include "tuneables.h"
#include "index.h"
#include "util.h"
#include "scan.h"


// stored length + offset
#define ENTRYSIZE (ObjectInfo::SIZE + 4 + 4)

Packfile::Packfile(const std::string &filename, packid_t id)
    : fd(-1), filename(filename), packid(id), numObjects(0), fileSize(0)
{
    fd = ::open(filename.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        perror("Packfile open");
        throw std::runtime_error("POSIX");
    }

    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        perror("Packfile fstat");
        throw std::runtime_error("POSIX");
    }

    fileSize = sb.st_size;
    
    // TODO: check for corruption?
}

Packfile::~Packfile()
{
    if (fd > 0)
        close(fd);
}

bool Packfile::full() const
{
    return numObjects >= PACKFILE_MAXOBJS ||
        fileSize >= PACKFILE_MAXSIZE;
}

// TODO: pack large objects contiguously
void
Packfile::addPayload(ObjectInfo info, const std::string &payload, Index *idx)
{
    assert(!full());
    assert(info.hasAllFields());

    lseek(fd, fileSize, SEEK_SET);
    // TODO: compression (detect when compression provides no benefit)
    // If compressed, change info

    // Headers
    strwstream headers;
    headers.writeInt<uint8_t>(1);

    offset_t newOffset = fileSize + 1 + 1 * ENTRYSIZE;
    headers.write(info.toString().data(), ObjectInfo::SIZE);
    headers.writeInt<uint32_t>(info.payload_size);
    headers.writeInt<offset_t>(newOffset);

    write(fd, headers.str().data(), headers.str().size());
    fileSize += headers.str().size();

    int bytesWritten = write(fd, payload.data(), payload.size());

    numObjects++;
    fileSize += bytesWritten;

    IndexEntry ie = {info, newOffset, bytesWritten, packid};
    idx->updateEntry(info.hash, ie);
}

bytestream *Packfile::getPayload(const IndexEntry &entry)
{
    assert(entry.packfile == packid); // TODO?
    return new fdstream(fd, entry.offset, entry.packed_size);
}

bool Packfile::purge(const ObjectHash &hash)
{
    NOT_IMPLEMENTED(false);
    return false;
}




/*
 * PackfileManager
 */

PackfileManager::PackfileManager(const std::string &rootPath)
    : rootPath(rootPath)
{
    if (!_loadFreeList()) {
        _recomputeFreeList();
        _writeFreeList();
    }
}

PackfileManager::~PackfileManager()
{
    _writeFreeList();
}

Packfile::sp
PackfileManager::getPackfile(packid_t id)
{
    if (!_packfileCache.hasKey(id)) {
        Packfile::sp pf(new Packfile(_getPackfileName(id), id));

        _packfileCache.put(id, pf);
        return pf;
    }

    return _packfileCache.get(id);
}

Packfile::sp
PackfileManager::newPackfile()
{
    assert(freeList.size() > 0);
    packid_t id = freeList[0];
    Packfile::sp pf(new Packfile(_getPackfileName(id), id));
    if (freeList.size() == 1) {
        freeList[0] += 1;
    }
    else {
        freeList.pop_front();
    }
    return pf;
}


int _freeListCB(void *ctx, const char *cpath)
{
    std::vector<packid_t> *existing = (std::vector<packid_t>*)ctx;
    std::string path = StrUtil_Basename(cpath);
    packid_t id = 0;
    if (sscanf(path.c_str(), "pack%u.pak", &id) != 1) {
        return 0;
    }
    existing->push_back(id);
    return 0;
}

void
PackfileManager::_recomputeFreeList()
{
    std::vector<packid_t> existing;
    Scan_Traverse(rootPath.c_str(), &existing, _freeListCB);
    std::sort(existing.begin(), existing.end());

    freeList.clear();

    if (existing.size() > 0) {
        packid_t curr = 0;
        size_t ix = 0;
        while (curr < existing.back()) {
            while (curr < existing[ix]) {
                freeList.push_back(curr);
                curr++;
            }
            curr++;
            ix++;
        }
        freeList.push_back(existing.back()+1);
    }
    else {
        freeList.push_back(0);
    }
}

bool
PackfileManager::_loadFreeList()
{
    std::string freeListPath = rootPath + PFMGR_FREELIST;
    int fd = ::open(freeListPath.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("PackfileManager::_loadFreeList open");
        return false;
    }

    freeList.clear();

    fdstream fs(fd, 0);
    uint32_t numEntries = fs.readInt<uint32_t>();
    if (fs.error()) return false;
    for (size_t i = 0; i < numEntries; i++) {
        packid_t id = fs.readInt<packid_t>();
        if (fs.error()) return false;
        freeList.push_back(id);
    }

    close(fd);

    return true;
}

void
PackfileManager::_writeFreeList()
{
    strwstream ss;
    ss.writeInt<uint32_t>(freeList.size());
    for (size_t i = 0; i < freeList.size(); i++) {
        packid_t id = freeList[i];
        ss.writeInt(id);
    }

    std::string freeListPath = rootPath + PFMGR_FREELIST;
    int fd = ::open(freeListPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("PackfileManager::_loadFreeList open");
        throw std::runtime_error("POSIX");
    }
    const std::string &str = ss.str();
    write(fd, str.data(), str.size());
    close(fd);
}

std::string
PackfileManager::_getPackfileName(packid_t id)
{
    std::stringstream ss;
    ss << rootPath;
    ss << "pack";
    ss << id;
    ss << ".pak";
    return ss.str();
}
