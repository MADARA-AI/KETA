#include <linux/kernel.h>
#include <linux/sched.h>
#include "memory_cache.h"

phys_addr_t translate_linear_address(struct mm_struct* mm, uintptr_t va);

bool read_physical_address(phys_addr_t pa, void* buffer, size_t size);

bool write_physical_address(phys_addr_t pa, void* buffer, size_t size);

bool read_process_memory(pid_t pid, uintptr_t addr, void* buffer, size_t size);

bool write_process_memory(pid_t pid, uintptr_t addr, void* buffer, size_t size);

void memory_cache_init_global(void);
void memory_cache_destroy_global(void);
