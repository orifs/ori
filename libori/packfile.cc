#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdexcept>

#include "packfile.h"
#include "tuneables.h"


// stored length + offset
#define ENTRYSIZE (ObjectInfo::SIZE + 4 + 4)

Packfile::Packfile(const std::string &filename)
    : fd(-1), filename(filename), numObjects(0), fileSize(0)
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
Packfile::addPayload(const ObjectInfo &info, bytestream *data_str, Index *idx)
{
    assert(!full());

    lseek(fd, fileSize, SEEK_SET);
    // TODO: compression (detect when compression provides no benefit)

    // Headers
    strwstream headers;
    headers.writeInt<uint8_t>(1);

    offset_t newOffset = fileSize + 1 + 1 * ENTRYSIZE;
    headers.write(info.toString().data(), ObjectInfo::SIZE);
    headers.writeInt<uint32_t>(info.payload_size);
    headers.writeInt<offset_t>(newOffset);

    write(fd, headers.str().data(), headers.str().size());
    fileSize += headers.str().size();

    int bytesWritten = data_str->copyToFd(fd);

    numObjects++;
    fileSize += bytesWritten;

    IndexEntry ie = {info, filename, newOffset, bytesWritten};
    idx->updateInfo(info.hash, ie);
}

bytestream *Packfile::getPayload(const IndexEntry &entry)
{
    assert(entry.packfile == filename); // TODO?
    return new fdstream(fd, entry.offset, entry.packed_size);
}
