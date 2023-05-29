#include "blobfs.h"

ssize_t
write(int fd, const void *buf, size_t count)
{
    struct spdk_blobfs *fs;
    struct sp_blob *blob;
    struct spdk_inode *inode;
    off_t offset;
    ssize_t ret;

    blobfs = spdk_fs_get_ctx();
    if (!blobfs) {
        return -ENODEV;
    }

    blob = (struct sp_blob *)fd;
    if (!blob) {
        return -EBADF;
    }

    inode = __spdk_get_inode_by_id(blobfs, blob->inode_id);
    if (!inode) {
        return -EBADF;
    }

    offset = blob->offset;
    ret = spdk_blob_write(blob, buf, offset, count);
    if (ret >0) {
        blob->offset += ret;
    }

    __spdk_put_inode(inode);

    return ret;
}
