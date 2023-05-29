#include "blobfs.h"

int
chmod(const char *path, mode_t mode)
{
    struct blobfs *blobfs = g_blobfs;
    struct spdk_inode *inode;
    int rc;

    inode = __spdk_get_inode_by_path(blobfs, path, NULL);
    if (!inode) {
        return -ENOENT;
    }

    inode->mode = (inode->mode & ~0777) | (mode & 0777);

    rc = __spdk_update_inode(blobfs, inode);
    if (rc) {
        __spdk_put_inode(inode);
        return rc;
    }

    __spdk_put_inode(inode);

    return 0;
}
