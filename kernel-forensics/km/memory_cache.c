#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include "memory_cache.h"

void cache_init(struct mem_cache *cache) {
    int i;
    spin_lock_init(&cache->lock);
    cache->next_evict = 0;
    
    for (i = 0; i < CACHE_SIZE; i++) {
        cache->entries[i].phys_addr = 0;
        cache->entries[i].virt_addr = NULL;
        cache->entries[i].last_access = 0;
        cache->entries[i].valid = false;
    }
}

void *cache_lookup(struct mem_cache *cache, phys_addr_t phys) {
    unsigned long flags;
    void *result = NULL;
    int i;
    
    spin_lock_irqsave(&cache->lock, flags);
    
    // Simple hash-based lookup: use low bits of phys_addr as index
    int start_idx = (phys >> PAGE_SHIFT) % CACHE_SIZE;
    int idx = start_idx;
    
    // Check starting position first
    if (cache->entries[idx].valid && cache->entries[idx].phys_addr == phys) {
        cache->entries[idx].last_access = jiffies;
        result = cache->entries[idx].virt_addr;
        spin_unlock_irqrestore(&cache->lock, flags);
        return result;
    }
    
    // Linear probe if not found
    for (i = 1; i < CACHE_SIZE; i++) {
        idx = (start_idx + i) % CACHE_SIZE;
        if (cache->entries[idx].valid && cache->entries[idx].phys_addr == phys) {
            cache->entries[idx].last_access = jiffies;
            result = cache->entries[idx].virt_addr;
            break;
        }
    }
    
    spin_unlock_irqrestore(&cache->lock, flags);
    return result;
}

void cache_insert(struct mem_cache *cache, phys_addr_t phys, void *virt) {
    unsigned long flags;
    int idx;
    
    if (!virt) {
        return;
    }
    
    spin_lock_irqsave(&cache->lock, flags);
    
    idx = cache->next_evict;
    
    // Evict old entry if it exists
    if (cache->entries[idx].valid && cache->entries[idx].virt_addr) {
        iounmap(cache->entries[idx].virt_addr);
    }
    
    cache->entries[idx].phys_addr = phys;
    cache->entries[idx].virt_addr = virt;
    cache->entries[idx].last_access = jiffies;
    cache->entries[idx].valid = true;
    
    cache->next_evict = (cache->next_evict + 1) % CACHE_SIZE;
    
    spin_unlock_irqrestore(&cache->lock, flags);
}

void cache_destroy(struct mem_cache *cache) {
    unsigned long flags;
    int i;
    
    spin_lock_irqsave(&cache->lock, flags);
    for (i = 0; i < CACHE_SIZE; i++) {
        if (cache->entries[i].valid && cache->entries[i].virt_addr) {
            iounmap(cache->entries[i].virt_addr);
            cache->entries[i].virt_addr = NULL;
            cache->entries[i].valid = false;
        }
    }
    spin_unlock_irqrestore(&cache->lock, flags);
}
