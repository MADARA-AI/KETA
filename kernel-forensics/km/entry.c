#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/kallsyms.h>
#include "comm.h"
#include "memory.h"
#include "process.h"
#include "verify.h"
#include "rate_limit.h"

#define MAX_TRANSFER_SIZE (PAGE_SIZE * 16)

static struct rate_limiter rl;

static long cheat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    void __user *uarg = (void __user *)arg;
    COPY_MEMORY cm;
    MODULE_BASE mb;
    char name[0x100] = {0};
    void *kbuf = NULL;
    int ret = -EINVAL;

    if (current->ptrace & PT_PTRACED)
        return -EPERM;

    if (rate_limit_check(&rl) != 0)
        return -EBUSY;

    switch (cmd) {
        case OP_INIT_KEY:
            return 0;                     // always success

        case OP_READ_MEM:
            if (copy_from_user(&cm, uarg, sizeof(cm))) return -EFAULT;
            if (cm.size == 0 || cm.size > MAX_TRANSFER_SIZE) return -EINVAL;

            kbuf = kmalloc(cm.size, GFP_KERNEL);
            if (!kbuf) return -ENOMEM;

            if (!read_process_memory(cm.pid, cm.addr, kbuf, cm.size)) {
                ret = -EIO;
                goto out;
            }
            if (copy_to_user(cm.buffer, kbuf, cm.size))
                ret = -EFAULT;
            else
                ret = 0;
            break;

        case OP_WRITE_MEM:
            if (copy_from_user(&cm, uarg, sizeof(cm))) return -EFAULT;
            if (cm.size == 0 || cm.size > MAX_TRANSFER_SIZE) return -EINVAL;

            kbuf = kmalloc(cm.size, GFP_KERNEL);
            if (!kbuf) return -ENOMEM;

            if (copy_from_user(kbuf, cm.buffer, cm.size)) {
                ret = -EFAULT;
                goto out;
            }
            ret = write_process_memory(cm.pid, cm.addr, kbuf, cm.size) ? 0 : -EIO;
            break;

        case OP_MODULE_BASE:
            if (copy_from_user(&mb, uarg, sizeof(mb))) return -EFAULT;
            if (copy_from_user(name, (void __user*)mb.name, sizeof(name)-1))
                return -EFAULT;
            name[sizeof(name)-1] = '\0';

            mb.base = get_module_base(mb.pid, name);
            if (copy_to_user(uarg, &mb, sizeof(mb)))
                return -EFAULT;
            return 0;
    }

out:
    if (kbuf) kfree(kbuf);
    return ret;
}

static const struct file_operations cheat_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = cheat_ioctl,
    .compat_ioctl   = cheat_ioctl,
};

static struct miscdevice cheat_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .fops  = &cheat_fops,
};

static void hide_module_full(void)
{
    struct module *mod = THIS_MODULE;

    list_del(&mod->list);                    // hide from lsmod
    mod->name[0] = '\0';                     // wipe name

    kobject_del(&mod->mkobj.kobj);           // remove from sysfs
    if (mod->holders_dir) {
        kobject_del(mod->holders_dir);
        mod->holders_dir = NULL;
    }

    mod->sect_attrs = NULL;
    mod->notes_attrs = NULL;

    printk(KERN_INFO "[+] Module fully stealth hidden\n");
}

int __init driver_entry(void)
{
    unsigned int seed;
    memory_cache_init_global();
    rate_limiter_init(&rl, 120);               // very low = safer

    get_random_bytes(&seed, sizeof(seed));
    snprintf(cheat_misc.name, sizeof(cheat_misc.name), "diag%08x", seed);

    if (misc_register(&cheat_misc) < 0) {
        printk(KERN_ERR "[-] misc_register failed\n");
        return -1;
    }

    hide_module_full();
    printk(KERN_INFO "[+] Stealth device ready → /dev/%s", cheat_misc.name);
    return 0;
}

void __exit driver_unload(void)
{
    misc_deregister(&cheat_misc);
    memory_cache_destroy_global();
}

module_init(driver_entry);
module_exit(driver_unload);

MODULE_DESCRIPTION("diag");
MODULE_LICENSE("GPL");
