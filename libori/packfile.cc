#include "packfile.h"

Packfile::Packfile(const std::string &filename)
    : fd(-1)
{
    fd = ::open(filename.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        perror("Packfile open");
        throw std::runtime_error("POSIX");
    }

    // Is file newly-created?
    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        perror("Packfile fstat");
        throw std::runtime_error("POSIX");
    }

    // Read the file
    fdstream fs(fd, 0, 0);
}

Packfile::~Packfile()
{
    if (fd > 0)
        close(fd);
}
