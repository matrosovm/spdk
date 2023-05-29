#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "blobfs.h"

static struct spdk_inode*
__spdk_get_inode_by_path(struct spdk_blobfs *fs, const char *path)
{
    struct spdk_inode *inode = NULL;
    char *path_copy, *token, *saveptr;

    path_copy = strdup(path);
    if (!path_copy) {
        return NULL;
    }

    token = strtok_r(path_copy, "/", &saveptr);
    while (token) {
        if (!inode) {
            inode = spdk_fs_get_root_inode(fs);
        }

        if (inode->type != SPDK_INODE_DIR) {
            __spdk_put_inode(inode);
            free(path_copy);
            return NULL;
        }

        inode = __spdk_get_child_inode_by_name(fs, inode, token);
        if (!inode) {
            free(path_copy);
            return NULL;
        }

        token = strtok_r(NULL, "/", &saveptr);
    }

    free(path_copy);
    return inode;
}

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
