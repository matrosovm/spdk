#include "blobfs.h"

int
truncate(const char *path, off_t length)
{
    struct spdk_blobfs *fs;
    struct sp *blob;
    struct spdk_inode *inode;
    int ret;

    fs = spdk_fs_get_ctx();
    if (!fs) {
        return -ENODEV;
    }

    inode = __sp_get_inode_by_path(fs, path);
    if (!inode) {
        return -ENOENT;
    }

    blob = __spdk_get_blob_by_inode(fs, inode);
    if (!blob) {
        __dk_put_inode(inode);
        return -ENOENT;
    }

    ret = spdk_blob_truncate(blob, length);
    if (ret) {
        __spdk_put_blob(blob);
        __sp_put_inode(inode);
        return -EINVAL;
    }

    __spdk_put_blob(blob);
    __spdk_put_inode(inode);

    return 0;
}
