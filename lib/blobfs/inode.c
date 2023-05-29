#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "blobfs.h"

enum spdk_inode_type {
    SPDK_INODE_FILE,
    SPDK_INODE_DIR,
    SPDK_INODE_SYMLINK,
    SPDK_INODE_DEVICE,
    SPDK_INODE_SOCKET,
    SPDK_INODE_FIFO,
};


struct spdk_inode {
    mode_t mode;
    uid_t uid;
    gid_t gid;
    time_t atime;
    time_t mtime;
    time_t ctime;
    off_t size;
    int blocks_count;
    uint64_t nlink;
    uint64_t data[32];
    enum spdk_inode_type type;
};

struct cache_entry {
    uint64_t key;
    void *data;
    time_t last_access;
    struct spdk_inode *inode;
    uint32_t hash;    
    struct cache_entry *prev;
    struct cache_entry *next;
};

struct cache {
    size_t size;
    size_t count;
    struct cache_entry *head;
    struct cache_entry *tail;
};

struct superblock {
    uint32_t magic;
    uint32_t version;
    uint64_t size;
    uint64_t block_count;
    uint64_t block_size;
    uint64_t inode_count;
    uint64_t inode_table_block;
    uint64_t data_block;
    uint64_t data_block_count;
    uint64_t free_block_count;
    uint64_t free_block_bitmap;
    uint64_t dir_block;
    uint64_t inode_blob;
    uint64_t inode_channel;
    uint64_t last_ino;
    uint64_t first_ino;
    uint64_t inode_table_offset;
    uint64_t inode_cache;
};


struct blobfs {
    struct spdk_file *spdk_blob_fs;
    struct superblock *sb;
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

static void*
__cache(struct cache *cache, uint64_t key)
{
    struct cache_entry *entry;
    entry = __cache_find_entry(cache, key);

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

    entry = __cache_find_entry_by_data(cache, data);
    if (entry) {
        entry->last_access = time(NULL);
    }
}

void* 
__cache_lookup(struct cache* cache, uint64_t key)
{
    struct cache_entry* entry = cache->head;

    while (entry != NULL) {
        if (entry->key == key) {
            entry->last_access = time(NULL);

            if (entry != cache->head) {
                entry->prev->next = entry->next;
                if (entry->next != NULL) {
                    entry->next->prev = entry->prev;
                } else {
                    cache->tail = entry->prev;
                }
                entry->next = cache->head;
                entry->prev = NULL;
                cache->head->prev = entry;
                cache->head = entry;
            }

            return entry->data;
        }

        entry = entry->next;
    }

    return NULL;
}

int 
__cache_update(struct cache* cache, uint64_t key, void* data)
{
    struct cache_entry* entry = cache->head;

    while (entry != NULL) {
        if (entry->key == key) {
            entry->data = data;
            entry->last_access = time(NULL);

            if (entry != cache->head) {
                entry->prev->next = entry->next;
                if (entry->next != NULL) {
                    entry->next->prev = entry->prev;
                } else {
                    cache->tail = entry->prev;
                }
                entry->next = cache->head;
                entry->prev = NULL;
                cache->head->prev = entry;
                cache->head = entry;
            }

            return 0;
        }

        entry = entry->next;
    }

    return -1;
}


static struct spdk_inode*
__spdk_cache_lookup_inode(struct blobfs* blobfs, uint64_t ino)
{
    struct spdk_inode *inode;

    inode = (struct spdk_inode *)__cache_lookup(blobfs->sb->inode_cache, ino);
    if (inode) {
        __cache_update(blobfs->sb->inode_cache, inode);
    }

    return inode;
}


#define SPDK_CACHE_NUM_BUCKETS 1024
static
uint32_t __spdk_cache_inode_hash(uint64_t ino)
{
    // using fnv hashing
    uint32_t hash = 538;
    uint8_t *p = (uint8_t *)&ino;
    int i;

    for (i = 0; i < sizeof(ino); i++) {
        hash = ((hash << 5) + hash) + (*p++);
    }

    return hash % SPDK_CACHE_NUM_BUCKETS;
}


static
void __spdk_cache_add_inode(struct blobfs *blobfs, struct spdk_inode *inode)
{
    uint32_t hash = __spdk_cache_inode_hash(inode->uid);
    struct cache_entry *entry = calloc(1, sizeof(struct cache_entry*));

    if (!entry) {
        free(inode);
        return;
    }

    entry->inode = inode;
    entry->hash = hash;

    pthread_mutex_lock(&blobfs->spdk_blob_fs->lock);
    TAILQ_INSERT_TAIL(&blobfs->cache[hash], entry, link);
    pthread_mutex_unlock(&blobfs->spdk_blob_fs->lock);
}


static struct spdk_inode*
__spdk_cache_get_inode(struct blob *blobfs, uint64_t ino)
{
    struct spdk_inode *inode;

    inode = __spdk_cache_lookup_inode(blobfs, ino);
    if (inode) {
        return inode;
    }

    inode = __spdk_read_inode(blobfs, ino);
    if (!inode) {
        return NULL;
    }

    __spdk_cache_add_inode(blobfs, inode);

    return inode;
}

static uint64_t
__spdk_inode_offset(struct blobfs *blobfs, uint64_t ino)
{
    uint64_t offset;
    offset = blobfs->sb->inode_table_offset +
             (ino - blobfs->sb->first_ino) * sizeof(struct spdk_inode);

    return offset;
}

static void 
__spdk_read_inode_complete(struct blobfs *blobfs, void *cb, int bserrno)
{
    struct spdk_inode *inode = cb;

    if (bserrno) {
        free(inode);
        return;
    }

    __spdk_cache_add_inode(blobfs, inode);
}


static struct spdk_inode*
__spdk_read_inode(struct blobfs *blobfs, uint64_t ino)
{
    struct spdk_inode *inode;
    uint64_t offset;

    if (ino < blobfs->sb->first_ino || ino > blobfs->sb->last_ino) {
        return NULL;
    }

    inode = __spdk_cache_get_inode(blobfs, ino);
    if (inode) {
        return inode;
    }

    offset = __spdk_inode_offset(blobfs, ino);
    inode = calloc(1, sizeof(struct spdk_inode));
    if (!inode) {
        return NULL;
    }

    spdk_blob_io_read(blobfs->spdk_blob_fs, blobfs->sb->inode_channel, inode, 
                        offset, sizeof(*inode), __spdk_read_inode_complete, inode);

    return inode;
}

