#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "blobfs.h"

struct cache_entry {
    uint64_t key;
    void *data;
    time_t last_access;
    struct cache_entry *prev;
    struct cache_entry *next;
};

struct cache {
    size_t size;
    size_t count;
    struct cache_entry *head;
    struct cache_entry *tail;
};

static struct cache_entry*
__cache_find_entry(struct cache *cache, uint64_t key)
{
    struct cache_entry *entry;

    for (entry = cache->head; entry != NULL; entry = entry->next) {
        if (entry->key == key) {
            return entry;
        }
    }

    return NULL;
}

static struct cache_entry*
__cache_find_entry_by_data(struct cache *cache, void *data)
{
    struct cache_entry *entry;

    for (entry = cache->head; entry != NULL; entry = entry->next) {
        if (entry->data == data) {
            return entry;
        }
    }

    return NULL;
}

static void *
__cache(struct cache *cache, uint64_t key)
{
    struct cache_entry *entry;
    entry = cache_find_entry(cache, key);

    if (entry) {
        entry->last_access = time(NULL);
        return entry->data;
    }

    return NULL;
}

static void
__cache_update(struct cache *cache, void *data)
{
    struct cache_entry *entry;

    entry = cache_find_entry_by_data(cache, data);
    if (entry) {
        entry->last_access = time(NULL);
    }
}

static struct blobfs_inode*
__spdk_cache_lookup_inode(struct *blobfs, uint64_t ino)
{
    struct blobfs_inode *inode;

    inode = (struct blobfs_inode *)cache_lookup(blobfs->inode_cache, ino);
    if (inode) {
        cache_update(blobfs->inode_cache, inode);
    }

    return inode;
}

static struct blobfs_inode *
__spdk_cache_get_inode(struct blob *blobfs, uint64_t ino)
{
    struct blobfs_inode *inode;

    inode = blobfs_cache_lookup_inode(blobfs, ino);
    if (inode) {
        return inode;
    }

    inode = blobfs_read_inode(blobfs, ino);
    if (!inode) {
        return NULL;
    }

    blobfs_cache_add_inode(blobfs, inode);

    return inode;
}

static uint64_t
__blobfs_inode_offset(struct blobfs *blobfs, uint64_t ino)
{
    uint64_t offset;
    offset = blobfs->superblock.inode_table_offset +
             (ino - blobfs->superblock.first_ino) * sizeof(struct spdk_inode);

    return offset;
}

static struct blobfs_inode *
blobfs_read_inode(struct blobfs *blobfs, uint64_t ino{
    struct blobfs_inode *inode;
    uint64_t offset;

    if (ino < blobfs->superblock.first_ino || ino > blobfs->superblock.last_ino) {
        return NULL;
    }

    inode = blobfs_cache_get_inode(blobfs, ino);
    if (inode) {
        return inode;
    }

    offset = blobfs_inode_offset(blobfs, ino);
    inode calloc(1, sizeof(*inode));
    if (!inode) {
        return NULL;
    }

    if (spdk_blob_io_read(blobfs->inode_blob, blobfs->inode_channel, inode,
                          sizeof(*inode), blobfs_read_inode_complete, inode)) {
        free(inode);
        return NULL;
    }

    return inode;
}

