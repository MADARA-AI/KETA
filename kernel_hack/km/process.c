#include "process.h"
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/mm.h>
#include <linux/version.h>

#define ARC_PATH_MAX 256

extern struct mm_struct *get_task_mm(struct task_struct *task);

#if(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 61))
extern void mmput(struct mm_struct *);
#endif

uintptr_t get_module_base(pid_t pid, char* name) 
{
    struct pid* pid_struct;
    struct task_struct* task;
    struct mm_struct* mm;
    struct vm_area_struct *vma;
    uintptr_t result = 0;

    if (!name || !name[0]) {
        return 0;
    }

    pid_struct = find_get_pid(pid);
    if (!pid_struct) {
        return 0;
    }
    task = get_pid_task(pid_struct, PIDTYPE_PID);
    put_pid(pid_struct);  // Release pid reference
    if (!task) {
        return 0;
    }
    mm = get_task_mm(task);
    put_task_struct(task);  // Release task reference after getting mm
    if (!mm) {
        return 0;
    }

    down_read(&mm->mmap_lock);
    
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
    // Kernel 6.1+ uses maple tree instead of linked list
    {
        VMA_ITERATOR(vmi, mm, 0);
        for_each_vma(vmi, vma) {
            char buf[ARC_PATH_MAX];
            char *path_nm;

            if (vma->vm_file) {
                path_nm = file_path(vma->vm_file, buf, ARC_PATH_MAX - 1);
                if (!IS_ERR(path_nm) && !strcmp(kbasename(path_nm), name)) {
                    result = vma->vm_start;
                    break;
                }
            }
        }
    }
#else
    for (vma = mm->mmap; vma; vma = vma->vm_next) {
        char buf[ARC_PATH_MAX];
        char *path_nm;

        if (vma->vm_file) {
            path_nm = file_path(vma->vm_file, buf, ARC_PATH_MAX - 1);
            if (!IS_ERR(path_nm) && !strcmp(kbasename(path_nm), name)) {
                result = vma->vm_start;
                break;
            }
        }
    }
#endif

    up_read(&mm->mmap_lock);
    mmput(mm);  // Release mm reference at the end
    return result;
}
 