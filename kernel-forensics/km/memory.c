#include "memory.h"
#include "memory_cache.h"
#include <linux/tty.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/random.h>

#include <asm/cpu.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/pgtable.h>

extern struct mm_struct *get_task_mm(struct task_struct *task);

static struct mem_cache global_mem_cache;

void memory_cache_init_global(void) {
    cache_init(&global_mem_cache);
}

void memory_cache_destroy_global(void) {
    cache_destroy(&global_mem_cache);
}

#if(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 61))
extern void mmput(struct mm_struct *);

phys_addr_t translate_linear_address(struct mm_struct* mm, uintptr_t va) {

    pgd_t *pgd;
    p4d_t *p4d;
    pmd_t *pmd;
    pte_t *pte;
    pud_t *pud;
	
    phys_addr_t page_addr;
    uintptr_t page_offset;
    
    pgd = pgd_offset(mm, va);
    if(pgd_none(*pgd) || pgd_bad(*pgd)) {
        return 0;
    }
    p4d = p4d_offset(pgd, va);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
    	return 0;
    }
	pud = pud_offset(p4d,va);
	if(pud_none(*pud) || pud_bad(*pud)) {
        return 0;
    }
	pmd = pmd_offset(pud,va);
	if(pmd_none(*pmd)) {
        return 0;
    }
	pte = pte_offset_kernel(pmd,va);
	if(pte_none(*pte)) {
        return 0;
    }
	if(!pte_present(*pte)) {
        return 0;
    }
	//页物理地址
	page_addr = (phys_addr_t)(pte_pfn(*pte) << PAGE_SHIFT);
	//页内偏移
	page_offset = va & (PAGE_SIZE-1);
	
	return page_addr + page_offset;
}
#else
phys_addr_t translate_linear_address(struct mm_struct* mm, uintptr_t va) {

    pgd_t *pgd;
    pmd_t *pmd;
    pte_t *pte;
    pud_t *pud;
	
    phys_addr_t page_addr;
    uintptr_t page_offset;
    
    pgd = pgd_offset(mm, va);
    if(pgd_none(*pgd) || pgd_bad(*pgd)) {
        return 0;
    }
	pud = pud_offset(pgd,va);
	if(pud_none(*pud) || pud_bad(*pud)) {
        return 0;
    }
	pmd = pmd_offset(pud,va);
	if(pmd_none(*pmd)) {
        return 0;
    }
	pte = pte_offset_kernel(pmd,va);
	if(pte_none(*pte)) {
        return 0;
    }
	if(!pte_present(*pte)) {
        return 0;
    }
	//页物理地址
	page_addr = (phys_addr_t)(pte_pfn(*pte) << PAGE_SHIFT);
	//页内偏移
	page_offset = va & (PAGE_SIZE-1);
	
	return page_addr + page_offset;
}
#endif

#ifndef ARCH_HAS_VALID_PHYS_ADDR_RANGE
static inline int valid_phys_addr_range(phys_addr_t addr, size_t count) {
    return addr + count <= __pa(high_memory);
}
#endif

bool read_physical_address(phys_addr_t pa, void* buffer, size_t size) {
    void* mapped;
    phys_addr_t page_addr;

    if (!pfn_valid(__phys_to_pfn(pa))) {
        return false;
    }
    if (!valid_phys_addr_range(pa, size)) {
        return false;
    }
    
    page_addr = pa & PAGE_MASK;
    
    mapped = cache_lookup(&global_mem_cache, page_addr);
    if (!mapped) {
        mapped = ioremap_cache(page_addr, PAGE_SIZE);
        if (!mapped) {
            return false;
        }
        cache_insert(&global_mem_cache, page_addr, mapped);
    }
    
    if(copy_to_user(buffer, mapped + (pa & ~PAGE_MASK), size)) {
        return false;
    }
    return true;
}

bool write_physical_address(phys_addr_t pa, void* buffer, size_t size) {
    void* mapped;
    phys_addr_t page_addr;

    if (!pfn_valid(__phys_to_pfn(pa))) {
        return false;
    }
    if (!valid_phys_addr_range(pa, size)) {
        return false;
    }
    
    page_addr = pa & PAGE_MASK;
    
    mapped = cache_lookup(&global_mem_cache, page_addr);
    if (!mapped) {
        mapped = ioremap_cache(page_addr, PAGE_SIZE);
        if (!mapped) {
            return false;
        }
        cache_insert(&global_mem_cache, page_addr, mapped);
    }
    
    if(copy_from_user(mapped + (pa & ~PAGE_MASK), buffer, size)) {
        return false;
    }
    return true;
}


static inline unsigned long size_inside_page(unsigned long start,
					     unsigned long size) {
	unsigned long sz;
	sz = PAGE_SIZE - (start & (PAGE_SIZE - 1));
	return min(sz, size);
}

static inline bool should_stop_iteration(void) {
	if (need_resched())
		cond_resched();
	return signal_pending(current);
}

static long x_probe_kernel_read(void *dst, const char *src, size_t size) {
#if(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 61))
    return copy_from_kernel_nofault(dst, src, size);
#else
    return probe_kernel_read(dst, src, size);
#endif
}

static long x_probe_kernel_write(void *dst, const char *src, size_t size) {
#if(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 61))
    return copy_to_kernel_nofault(dst, src, size);
#else
    return probe_kernel_write(dst, src, size);
#endif
}


bool read_process_memory(
    pid_t pid, 
    uintptr_t addr, 
    void* buffer, 
    size_t size) {
    
    struct task_struct* task;
    struct mm_struct* mm;
    struct pid* pid_struct;
    phys_addr_t pa;
    size_t bytes_read = 0;
    uintptr_t current_addr;
    size_t page_offset;
    size_t bytes_in_page;
    bool result = false;

    pid_struct = find_get_pid(pid);
    if (!pid_struct) {
        return false;
    }
    task = get_pid_task(pid_struct, PIDTYPE_PID);
    put_pid(pid_struct);  // Release pid reference
    if (!task) {
        return false;
    }
    mm = get_task_mm(task);
    put_task_struct(task);  // Release task reference after getting mm
    if (!mm) {
        return false;
    }
    
    // Hold mm lock while accessing
    down_read(&mm->mmap_lock);
    
    while (bytes_read < size) {
        current_addr = addr + bytes_read;
        page_offset = current_addr & ~PAGE_MASK;
        bytes_in_page = PAGE_SIZE - page_offset;
        if (bytes_in_page > size - bytes_read) {
            bytes_in_page = size - bytes_read;
        }
        
        pa = translate_linear_address(mm, current_addr);
        if (!pa) {
            goto out;
        }
        
        if (!read_physical_address(pa, (char*)buffer + bytes_read, bytes_in_page)) {
            goto out;
        }
        
        bytes_read += bytes_in_page;
        udelay(400 + (get_random_u32() % 1800));  // 0.4–2.2 ms per page
        if (get_random_u32() % 100 < 8)
            mdelay(5 + get_random_u32() % 15);   // occasional 5–20 ms stall
    }
    result = true;

out:
    up_read(&mm->mmap_lock);
    mmput(mm);  // Release mm reference at the end
    return result;
}

bool write_process_memory(
    pid_t pid, 
    uintptr_t addr, 
    void* buffer, 
    size_t size) {
    
    struct task_struct* task;
    struct mm_struct* mm;
    struct pid* pid_struct;
    phys_addr_t pa;
    size_t bytes_written = 0;
    uintptr_t current_addr;
    size_t page_offset;
    size_t bytes_in_page;
    bool result = false;

    pid_struct = find_get_pid(pid);
    if (!pid_struct) {
        return false;
    }
    task = get_pid_task(pid_struct, PIDTYPE_PID);
    put_pid(pid_struct);  // Release pid reference
    if (!task) {
        return false;
    }
    mm = get_task_mm(task);
    put_task_struct(task);  // Release task reference after getting mm
    if (!mm) {
        return false;
    }
    
    // Hold mm lock while accessing
    down_read(&mm->mmap_lock);
    
    while (bytes_written < size) {
        current_addr = addr + bytes_written;
        page_offset = current_addr & ~PAGE_MASK;
        bytes_in_page = PAGE_SIZE - page_offset;
        if (bytes_in_page > size - bytes_written) {
            bytes_in_page = size - bytes_written;
        }
        
        pa = translate_linear_address(mm, current_addr);
        if (!pa) {
            goto out;
        }
        
        if (!write_physical_address(pa, (char*)buffer + bytes_written, bytes_in_page)) {
            goto out;
        }
        
        bytes_written += bytes_in_page;
        udelay(400 + (get_random_u32() % 1800));  // 0.4–2.2 ms per page
        if (get_random_u32() % 100 < 8)
            mdelay(5 + get_random_u32() % 15);   // occasional 5–20 ms stall
    }
    result = true;

out:
    up_read(&mm->mmap_lock);
    mmput(mm);  // Release mm reference at the end
    return result;
}
