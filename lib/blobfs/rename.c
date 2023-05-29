#include "blobfs.h"

// TODO
int
rmdir(const char *path)
{
    struct spdk_blobfs *blobfs;
    struct spdk_blob *blob;
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

    if (!spdk_blob_is_empty(blob)) {
        spdk_blob_close(blob, NULL, NULL);
        __spdk_put_inode(inode);
        free(parent_path);
        free(name);
        return -ENOTEMPTY;
    }

    spdk_blob_close(blob, NULL, NULL);
    __spdk_remove_blob_from_parent(inode, name);
    __spdk_put_inode(inode);
    free(parent_path);
    free(name);

    return 0;
}