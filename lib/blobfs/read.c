#include <fcntl.h>
#include <unistd.h>
#include <spdk/blobfs.h>

ssize_t read(int fd, void *buf, size_t count)
{
    struct spdk_blobfs *blobfs = g_blobfs;
    struct spdk_file *file;
    ssize_t rc;

    spdk_env_opts_init(NULL);
    spdk_env_init();

    if (!blobfs) {
        return 0;
    }  

    file = __spdk_file_from_fd(fd);
    if (!file) {
        return 0;
    }

    rc = spdk_file_read(file, buf, count, 0, 1024);
    if (rc < 0) {
        return 0;
    }

    return rc;
}
