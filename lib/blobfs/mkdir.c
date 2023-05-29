#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "blobfs.h"

int blobfs_mkdir(const char *path, mode_t mode)
{
    struct blobfs *blobfs = g_blobfs;
    struct spdk_inode *parent_inode, *inode;
    char *name;
    int rc;

    parent_inode = __spdk_get_inode_by_path(blobfs, path, &name);
    if (!parent_inode) {
        return -ENOENT;
    }

    if (__spdk_get_inode_by_name(blobfs, parent_inode, name)) {
        __spdk_put_inode(parent_inode);
        free(name);
        return -EEXIST;
    }

    inode = blobfs_alloc_inode(blobfs);
    if (!inode) {
        blobfs_put_inode(parent_inode);
        free(name);
        return -ENOSPC;
    }

    inode->mode = S_IFDIR | mode;
    inode->nlink = 2;

    rc = __spdk_add_link(blobfs, parent_inode, inode, name);
    if (rc) {
        __spdk_free_inode(blobfs, inode);
        __spdk_put_inode(parent_inode);
        free(name);
        return rc;
    }

    __spdk_put_inode(inode);
    __spdk_put_inode(parent_inode);
    free(name);

    return 0;
}
