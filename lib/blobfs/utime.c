#include <utime.h>
#include "blobfs.h"

int 
utime(const char *path, const struct utimbuf *times)
{
    struct spdk_blobfs *blobfs;
    struct sp_blob *blob;
    struct spdk_inode *inode;
    char *parent_path, *name;
    int ret;

    blobfs = spdk_fs_get_ctx();
    if (!blobfs) {
        return -ENODEV;
    }

    ret = spdk_fs_parse_path(path, &parent_path, &name);
    if (ret) {
        return ret;
    }

    inode = __spdk_get_inode_by_path(blobfs, parent_path);
    if (!inode) {
        free(parent_path);
        free(name);
        return -ENOENT;
    }

    blob = __spdk_get_blob_by_name(blobfs, inode, name);
    if (!blob) {
        __spdk_put_inode(inode);
        free(parent_path);
        free(name);
        return -ENOENT;
    }

    if (times) {
        spdk_blob_set_xattr(blob, "user.utime.actime", &times->actime, sizeof(times->actime));
        spdk_blob_set_xattr(blob, "user.utime.modtime", &times->modtime, sizeof(times->modtime));
    } else {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        spdk_blob_set_xattr(blob, "user.utime.actime", &tv.tv_sec, sizeof(tv.tv_sec));
        spdk_blob_set_xattr(blob, "user.utime.modtime", &tv.tv_sec, sizeof(tv.tv_sec));
    }

    spdk_blob_close(blob, NULL, NULL);
    __spdk_put_inode(inode);
    free(parent_path);
    free(name);

    return 0;
}
