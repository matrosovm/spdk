#include "blobfs.h"

// TODO
int stat(const char *path, struct stat *st)
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

    memset(st, 0, sizeof(struct stat));
    st->st_mode = S_IFREG | 0666;
    st->st_nlink = 1;
    st->st_size = spdk_blob_get_num_clusters(blob) * spdk_bs_get_cluster_size(fs->bs);
    st->st_blocks = st->st_size / 512;
    st->st_blksize = 512;
    st->st_atime = spdk_blob_get_snapshot(blob)->create_time;
    st->st_mtime = spdk_blob_get_snapshot(blob)->create_time;
    st->st_ctime = spdk_blob_get_snapshot(blob)->create_time;

    __spdk_put_blob(blob);
    __spdk_put_inode(inode);

    return 0;
}
