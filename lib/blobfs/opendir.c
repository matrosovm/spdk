#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "blobfs.h"

static void
__spdk_put_inode(struct spdk_inode *inode)
{
    if (!inode) {
        return;
    }

    if (inode->nlink > 1) {
        inode->nlink--;
        return;
    }

    __spdk_remove_inode_from_cache(inode);

    if (inode->type == SPDK_INODE_TYPE_DIRECTORY) {
        __spdk_free_directory(inode->data);
    } else if (inode->type == SPDK_INODE_TYPE_REGULAR) {
        __spdk_free_file(inode->data);
    }

    spdk_free(inode);
}


static struct spdk_inode*
__spdk_get_inode_by_id(struct blobfs *blobfs, uint64_t inode_id)
{
    struct spdk_blob *blob;
    struct spdk_inode *inode;

    blob = __spdk_get_blob_by_id(blobfs, inode_id);
    if (!blob) {
        return NULL;
    }

    inode = __spdk_blob_to_inode(blob);
    if (!inode) {
        spdk_blob_close(blob);
        return NULL;
    }

    spdk_blob_close(blob);
    return inode;
}


static struct spdk_inode*
__spdk_get_inode_by_name(struct blobfs *blobfs, struct spdk_inode *parent, const char *name)
{
    struct spdk_blob *blob;
    struct spdk_inode *inode;
    uint64_t inode_id;

    blob = __spdk_get_blob_by_name(blobfs, parent, name);
    if (!blob) {
        return NULL;
    }

    inode_id = spdk_blob_get_xattr_value_uint64(blob, "inode");
    if (!inode_id) {
        spdk_blob_close(blob);
        return NULL;
    }

    inode = __spdk_get_inode_by_id(blobfs, inode_id);
    if (!inode) {
        spdk_blob_close(blob);
        return NULL;
    }

    spdk_blob_close(blob);
    return inode;
}


static struct spdk_inode*
__spdk_get_root_inode(struct blobfs *blobfs)
{
    struct spdk_inode *inode;

    inode = __spdk_get_inode_by_name(blobfs, NULL, "/");
    if (!inode) {
        return NULL;
    }

    if ((inode->mode & S_IFMT) != S_IFDIR) {
        __spdk_put_inode(inode);
        return NULL;
    }

    return inode;
}


static struct spdk_inode*
__spdk_get_inode_by_path(struct blobfs *blobfs, const char *path, char **name)
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
            inode = __spdk_get_root_inode(blobfs);
        }

        if ((inode->mode & S_IFMT) != S_IFDIR) {
            __spdk_put_inode(inode);
            free(path_copy);
            return NULL;
        }

        inode = __spdk_get_inode_by_name(blobfs, inode, token);
        if (!inode) {
            free(path_copy);
            return NULL;
        }

        if (name && !strtok_r(NULL, "/", &saveptr)) {
            *name = strdup(token);
        }

        token = strtok_r(NULL, "/", &saveptr);
    }

    free(path_copy);
    return inode;
}

DIR
*opendir(const char *path)
{
    struct blobfs *blobfs = g_blobfs;
    struct spdk_inode *inode;
    DIR *dirp;

    inode = __spdk_get_inode_by_path(blobfs, path, NULL);
    if (inode) {
        return NULL;
    }

    if ((inode->mode & S_IFMT) != S_IFDIR) {
        __spdk_put_inode(inode);
        return NULL;
    }

    dirp = (DIR *)malloc(sizeof(DIR));
    if (!dirp) {
        __spdk_put_inode(inode);
        return NULL;
    }

    dirp->dd_fd = (int)(intptr_t)inode;
    dirp->dd_loc = 0;
    dirp->dd_size = 0;

    return dirp;
}
