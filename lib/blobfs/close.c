#include "blobfs.h"

static int
__spdk_close_file(struct blobfs *blobfs, struct spdk_file *file)
{
    int rc = __spdk_flush_file(blobfs, file);
    if (rc) {
        return rc;
    }

    rc = __spdk_free_file(blobfs, file);
    if (rc) {
        return rc;
    }

    return 0;
}


int
close(int fd)
{
    struct blobfs *blobfs = g_blobfs;
    struct spdk_file *file = (struct spdk_file*)fd;
    int rc;

    rc = __spdk_close_file(blobfs, file);
    if (rc) {
        return rc;
    }

    return 0;
}
