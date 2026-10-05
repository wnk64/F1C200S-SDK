#include "cedar_cma_pool.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PAGE_BYTES 4096u
#define SLOT_COUNT 128
#define POOL_LIMIT (16u * 1024u * 1024u)

struct slot { size_t offset, bytes; };
static struct {
    struct ScMemOpsS original;
    struct ScMemOpsS *table;
    unsigned char *memory;
    size_t capacity, used, peak;
    void *ve_ops, *ve_self;
    unsigned refs, allocations, fallbacks;
    int failed;
    struct slot slots[SLOT_COUNT];
} pool;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static int pool_open(void)
{
    int rc;
    pthread_mutex_lock(&lock);
    rc = pool.original.open();
    if (rc >= 0)
        pool.refs++;
    else if (!pool.refs) {
        *pool.table = pool.original;
        memset(&pool, 0, sizeof(pool));
    }
    pthread_mutex_unlock(&lock);
    return rc;
}

static void pool_close(void)
{
    pthread_mutex_lock(&lock);
    if (!pool.refs) {
        pthread_mutex_unlock(&lock);
        return;
    }
    if (--pool.refs == 0) {
        fprintf(stderr, "CMA pool: peak=%zu capacity=%zu allocations=%u fallback=%u live=%zu\n",
                pool.peak, pool.capacity, pool.allocations, pool.fallbacks, pool.used);
        /* Decoder teardown has finished before the final adapter close. */
        if (pool.memory)
            pool.original.pfree(pool.memory, pool.ve_ops, pool.ve_self);
        *pool.table = pool.original;
    }
    pool.original.close();
    if (!pool.refs)
        memset(&pool, 0, sizeof(pool));
    pthread_mutex_unlock(&lock);
}

static void *pool_alloc(int size, void *ve_ops, void *ve_self)
{
    size_t bytes, offset = 0;
    int slot = -1, moved;
    void *result;
    if (size <= 0)
        return NULL;
    bytes = ((size_t)size + PAGE_BYTES - 1) & ~(size_t)(PAGE_BYTES - 1);
    pthread_mutex_lock(&lock);
    if (!pool.memory && !pool.failed && bytes <= pool.capacity) {
        pool.memory = pool.original.palloc((int)pool.capacity, ve_ops, ve_self);
        if (pool.memory) {
            pool.ve_ops = ve_ops;
            pool.ve_self = ve_self;
            fprintf(stderr, "CMA pool: reserved=%zu bytes in one ION allocation\n", pool.capacity);
        } else {
            pool.failed = 1;
            fprintf(stderr, "CMA pool: reservation failed; using original allocator\n");
        }
    }
    if (!pool.memory || ve_ops != pool.ve_ops || ve_self != pool.ve_self)
        goto fallback;
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (!pool.slots[i].bytes) {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        goto fallback;
    /* First-fit holes, including freed blocks, without moving live DMA buffers. */
    do {
        moved = 0;
        if (offset > pool.capacity || bytes > pool.capacity - offset)
            goto fallback;
        for (int i = 0; i < SLOT_COUNT; i++) {
            struct slot *s = &pool.slots[i];
            if (s->bytes && offset < s->offset + s->bytes && offset + bytes > s->offset) {
                offset = s->offset + s->bytes;
                moved = 1;
                break;
            }
        }
    } while (moved);
    pool.slots[slot].offset = offset;
    pool.slots[slot].bytes = bytes;
    pool.used += bytes;
    if (pool.used > pool.peak)
        pool.peak = pool.used;
    pool.allocations++;
    result = pool.memory + offset;
    pthread_mutex_unlock(&lock);
    return result;
fallback:
    pool.fallbacks++;
    result = pool.original.palloc(size, ve_ops, ve_self);
    pthread_mutex_unlock(&lock);
    return result;
}

static void pool_free(void *memory, void *ve_ops, void *ve_self)
{
    uintptr_t address = (uintptr_t)memory;
    pthread_mutex_lock(&lock);
    if (pool.memory && address >= (uintptr_t)pool.memory &&
        address - (uintptr_t)pool.memory < pool.capacity) {
        size_t offset = address - (uintptr_t)pool.memory;
        for (int i = 0; i < SLOT_COUNT; i++) {
            struct slot *s = &pool.slots[i];
            if (s->bytes && s->offset == offset) {
                pool.used -= s->bytes;
                s->bytes = 0;
                pthread_mutex_unlock(&lock);
                return;
            }
        }
        fprintf(stderr, "CMA pool: rejected invalid or duplicate suballocation free\n");
    } else if (memory) {
        pool.original.pfree(memory, ve_ops, ve_self);
    }
    pthread_mutex_unlock(&lock);
}

int cedar_cma_pool_enable(struct ScMemOpsS *ops, size_t bytes)
{
    if (!ops || !ops->open || !ops->close || !ops->palloc || !ops->pfree ||
        !bytes || bytes > POOL_LIMIT || bytes % PAGE_BYTES)
        return -1;
    pthread_mutex_lock(&lock);
    if (pool.table) {
        pthread_mutex_unlock(&lock);
        return -1;
    }
    pool.original = *ops;
    pool.table = ops;
    pool.capacity = bytes;
    /* Cedar re-fetches this same table during InitializeVideoDecoder. */
    ops->open = pool_open;
    ops->close = pool_close;
    ops->palloc = pool_alloc;
    ops->pfree = pool_free;
    pthread_mutex_unlock(&lock);
    return 0;
}
