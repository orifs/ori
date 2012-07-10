#define FUSE_USE_VERSION 26
#include <fuse.h>

struct mount_ori_config {
    char *repo_path;
};

void mount_ori_parse_opt(struct fuse_args *args, mount_ori_config *conf);
