#ifndef _MEMORY_CACHE_H_
#define _MEMORY_CACHE_H_

#include <linux/types.h>
#include <linux/spinlock.h>

#define CACHE_SIZE 16

struct cache_entry {
    phys_addr_t phys_addr;
    void *virt_addr;
    unsigned long last_access;
    bool valid;
};

struct mem_cache {
    struct cache_entry entries[CACHE_SIZE];
    spinlock_t lock;
    int next_evict;
    
    // Statistics for debugging
    unsigned long hits;
    unsigned long misses;
};

void cache_init(struct mem_cache *cache);
void *cache_lookup(struct mem_cache *cache, phys_addr_t phys);
void cache_insert(struct mem_cache *cache, phys_addr_t phys, void *virt);
void cache_destroy(struct mem_cache *cache);
void cache_get_stats(struct mem_cache *cache, unsigned long *hits, unsigned long *misses);

#endif /* _MEMORY_CACHE_H_ */
