#include <sys/stat.h>
#include <fcntl.h>

#include <string>

#include "util.h"
#include "fuse_cmd.h"

std::string OF_ControlPath()
{
    char *cwdbuf = getcwd(NULL, 0);
    std::string path = cwdbuf;
    free(cwdbuf);

    while (path.size() > 0) {
        std::string control_path = path + "/" + ORI_CONTROL_FILENAME;
        if (Util_FileExists(control_path)) {
            return control_path;
        }
        path = StrUtil_Dirname(path);
    }

    return "";
}

bool OF_RunCommand(const char *cmd)
{
    std::string controlPath = OF_ControlPath();
    if (controlPath.size() == 0)
        return false;

    int fd = open(controlPath.c_str(), O_RDWR | O_TRUNC);
    if (fd < 0) {
        perror("OF_RunCommand open");
        return false;
    }

    write(fd, cmd, strlen(cmd));
    fsync(fd);
    lseek(fd, 0, SEEK_SET);

    // Read output
    struct stat sb;
    fstat(fd, &sb);
    size_t left = sb.st_size;
    while (left > 0) {
        size_t to_read = left;
        if (to_read > 1024)
            to_read = 1024;

        char buf[1024];
        read(fd, buf, to_read);
        fwrite(buf, to_read, 1, stdout);
        left -= to_read;
    }

    return true;
}
