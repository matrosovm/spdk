#include "blobfs.h"

int
chown(const char *path, uid_t uid, gid_t gid)
{
    struct blobfs *blobfs = g_blobfs;
    struct spdk_inode *inode;
    int rc;

    inode = __spdk_get_inode_by_path(blobfs, path, NULL);
    if (!inode) {
        return -ENOENT;
    }

    inode->uid = uid;
    inode->gid = gid;

    rc = __spdk_update_inode(blobfs, inode);
    if (rc) {
        __spdk_put_inode(inode);
        return rc;
    }

    __spdk_put_inode(inode);

    return 0;
}
