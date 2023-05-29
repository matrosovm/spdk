#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "blobfs.h"

#define BLOBFS_NAME_MAX 256

struct spdk_dir_entry {
    uint64_t inode;             
    uint16_t name_len;          
    uint8_t type;               
    char name[BLOBFS_NAME_MAX];
};

struct blobfs_file {
    struct spdk_inode* inode;
    uint32_t position;
    int flags;
};

int
open(const char *pathname, int flags, mode_t mode)
{
    struct spdk_inode* root_inode;
    int ret = blobfs_read_inode(0, root_inode);
    if (ret != 0) {
        return ret;
    }

    char* components[BLOBFS_NAME_MAX];
    int num_components = 0;
    char* component = strtok(pathname, "/");
    while (component != NULL) {
        components[num_components++] = component;
        component = strtok(NULL, "/");
    }

    struct spdk_inode* inode = root_inode;
    for (int i = 0; i < num_components; i++) {
        struct spdk_dir_entry entry;
        ret = blobfs_find_dir_entry(&inode, components[i], &entry);
        if (ret != 0) {
            return ret;
        }

        ret = blobfs_read_inode(entry.inode, inode);
        if (ret != 0) {
            return ret;
        }
    }

    struct blobfs_file* file = malloc(sizeof(struct blobfs_file));
    if (file == NULL) {
        return -ENOMEM;
    }

    file->inode = &inode;
    file->position = 0;
    file->flags = flags;

    return (int) file;
}
