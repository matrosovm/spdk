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

static int
__spdk_read_inode(uint64_t inode_num, struct spdk_inode* inode)
{
    struct blobfs *blobfs = g_blobfs;
    uint64_t offset = inode_num * sizeof(struct spdk_inode{});
    int ret = spdk_blob_io_read(blobfs->spdk_blob_fs, blobfs->sb->inode_channel, 
            inode, offset, sizeof(struct spdk_inode), NULL, inode);
    return ret;
}

static int
__spdk_find_dir_entry(struct spdk_dir_entry *dir_inode, const char *name, uint64_t *entry_inode_num)
{
    struct blobfs *blobfs = g_blobfs;
    uint64_t offset = 0;
    struct spdk_dir_entry entry;
    int ret;

    while (1) {
        ret = spdk_blob_io_read(blobfs->spdk_blob_fs, blobfs->sb->inode_channel, &entry, dir_inode->size, sizeof(struct spdk_dir_entry));
        if (ret != 0) {
            return ret;
        }

        if (entry.inode_num == 0) {
            return -ENOENT;
        }

        if (strcmp(entry.name, name) == 0) {
            *entry_inode_num = entry.inode_num;
            return 0;
        }

        offset += sizeof(struct spdk_dir_entry);
    }
}


int
open(const char *pathname, int flags, mode_t mode)
{
    struct spdk_inode* root_inode;
    int ret = __spdk_read_inode(0, root_inode);
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
        ret = __spdk_find_dir_entry(&inode, components[i], &entry);
        if (ret != 0) {
            return ret;
        }

        ret = __spdk_read_inode(entry.inode, inode);
        if (ret != 0) {
            return ret;
        }
    }

    struct blobfs_file* file = malloc(sizeof(struct blobfs_file));
    if (file == NULL) {
        return -ENOMEM;
    }

    file->inode = inode;
    file->position = 0;
    file->flags = flags;

    return (int)file;
}