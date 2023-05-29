#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "blobfs.h"

struct dirent *readdir(DIR *dirp)
{
    struct spdk_dir *dir;
    struct spdk_dirent *ent;
    struct dirent *de;

    dir = spdk_dir_from_fd(d->dd_fd);
    if (!dir) {
        return NULL;
    }

    ent = spdk_dir_read(dir);
    if (!ent) {
        return NULL;
    }

    de = (struct dirent *)malloc(sizeof(struct dirent));
    if (!de) {
        return NULL;
    }

    de->d_ino = ent->ino;
    strncpy(de->d_name, ent->name, sizeof(de->d_name));
    de->d_name[sizeof(de->d_name) - 1] = '\0';

    return de;
}
