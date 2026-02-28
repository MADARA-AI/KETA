#include <linux/module.h>
#include <linux/tty.h>
#include <linux/random.h>
#include <linux/timekeeping.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include <linux/delay.h>
#include <net/netlink.h>
#include <net/genetlink.h>
#include "comm.h"
#include "memory.h"
#include "process.h"
#include "verify.h"
#include "rate_limit.h"

#define MAX_TRANSFER_SIZE (PAGE_SIZE * 16)
#define XOR_KEY 0xAA
#define XOR_STR(str) ({ \
    static char buf[sizeof(str)]; \
    static bool init = false; \
    if (!init) { \
        strcpy(buf, str); \
        for (int i = 0; buf[i]; i++) buf[i] ^= XOR_KEY; \
        init = true; \
    } \
    buf; \
})

#define NETLINK_CHEAT_FAMILY 29
#define NETLINK_CHEAT_GROUP 22

enum {
    CHEAT_ATTR_UNSPEC,
    CHEAT_ATTR_CMD,
    CHEAT_ATTR_DATA,
    CHEAT_ATTR_PAD,
    __CHEAT_ATTR_MAX,
};
#define CHEAT_ATTR_MAX (__CHEAT_ATTR_MAX - 1)

static struct nla_policy cheat_genl_policy[CHEAT_ATTR_MAX + 1] = {
    [CHEAT_ATTR_CMD]  = { .type = NLA_U32 },
    [CHEAT_ATTR_DATA] = { .type = NLA_BINARY, .len = MAX_TRANSFER_SIZE + sizeof(COPY_MEMORY) + 128 },
};

static struct genl_family cheat_genl_family = {
    .name       = XOR_STR("diag"),
    .version    = 1,
    .maxattr    = CHEAT_ATTR_MAX,
    .policy     = cheat_genl_policy,
    .module     = THIS_MODULE,
    .parallel_ops = true,
};

static struct rate_limiter rl;

static int cheat_do_operation(struct sk_buff *skb, struct genl_info *info)
{
    u32 cmd;
    struct nlattr *data_attr;
    void *data;
    size_t len;
    COPY_MEMORY cm;
    MODULE_BASE mb;
    char name[0x100] = {0};
    pid_t caller_pid = current->tgid;
    u32 random_delay;
    void *kbuf = NULL;
    int ret = -EINVAL;

    if (current->ptrace & PT_PTRACED) {
        return -EPERM;
    }

    if (rate_limit_check(&rl) != 0) {
        return -EBUSY;
    }

    if ((get_random_u32() % 100) < 5) {
        get_random_bytes(&random_delay, sizeof(random_delay));
        usleep_range(100, 500);
    }

    if (!info->attrs[CHEAT_ATTR_CMD] || !info->attrs[CHEAT_ATTR_DATA]) {
        return -EINVAL;
    }

    cmd = nla_get_u32(info->attrs[CHEAT_ATTR_CMD]);
    data_attr = info->attrs[CHEAT_ATTR_DATA];
    data = nla_data(data_attr);
    len = nla_len(data_attr);

    // === CRACKED: INIT_KEY always succeeds (no verification) ===
    if (cmd == OP_INIT_KEY) {
        return 0;   // ignore the key completely
    }

    // No more per-pid verified check - everything is allowed now
    switch (cmd) {
        case OP_READ_MEM:
            if (len < sizeof(cm)) return -EINVAL;
            memcpy(&cm, data, sizeof(cm));
            if (cm.pid <= 0 || cm.size == 0 || cm.size > MAX_TRANSFER_SIZE || cm.addr == 0)
                return -EINVAL;
            if (!cm.buffer || !access_ok(cm.buffer, cm.size))
                return -EFAULT;

            kbuf = kmalloc(cm.size, GFP_KERNEL);
            if (!kbuf) return -ENOMEM;

            if (!read_process_memory(cm.pid, cm.addr, kbuf, cm.size)) {
                ret = -EIO;
                goto cleanup;
            }
            if (copy_to_user(cm.buffer, kbuf, cm.size)) {
                ret = -EFAULT;
                goto cleanup;
            }
            ret = 0;
            break;

        case OP_WRITE_MEM:
            if (len < sizeof(cm)) return -EINVAL;
            memcpy(&cm, data, sizeof(cm));
            if (cm.pid <= 0 || cm.size == 0 || cm.size > MAX_TRANSFER_SIZE || cm.addr == 0)
                return -EINVAL;
            if (!cm.buffer || !access_ok(cm.buffer, cm.size))
                return -EFAULT;

            kbuf = kmalloc(cm.size, GFP_KERNEL);
            if (!kbuf) return -ENOMEM;

            if (copy_from_user(kbuf, cm.buffer, cm.size)) {
                ret = -EFAULT;
                goto cleanup;
            }
            if (!write_process_memory(cm.pid, cm.addr, kbuf, cm.size)) {
                ret = -EIO;
                goto cleanup;
            }
            ret = 0;
            break;

        case OP_MODULE_BASE:
            if (len < sizeof(mb)) return -EINVAL;
            memcpy(&mb, data, sizeof(mb));
            if (mb.pid <= 0) return -EINVAL;
            if (!mb.name || !access_ok(mb.name, 1)) return -EFAULT;

            if (copy_from_user(name, (void __user*)mb.name, sizeof(name) - 1))
                return -EFAULT;
            name[sizeof(name) - 1] = '\0';

            mb.base = get_module_base(mb.pid, name);

            // reply
            struct sk_buff *reply = genlmsg_new(256, GFP_KERNEL);
            if (!reply) return -ENOMEM;
            struct genlmsghdr *hdr = genlmsg_put_reply(reply, info);
            if (!hdr) {
                nlmsg_free(reply);
                return -ENOMEM;
            }
            if (nla_put_u64_64bit(reply, CHEAT_ATTR_DATA, mb.base, CHEAT_ATTR_PAD)) {
                nlmsg_free(reply);
                return -EMSGSIZE;
            }
            genlmsg_end(reply, hdr);
            return genlmsg_reply(reply, info);

        default:
            return -ENOTTY;
    }

cleanup:
    if (kbuf) kfree(kbuf);
    return ret;
}

static struct genl_ops cheat_genl_ops[] = {
    {
        .cmd  = 0,
        .doit = cheat_do_operation,
        .flags = 0,
    },
};

static int init_netlink(void) {
    return genl_register_family_with_ops(&cheat_genl_family, cheat_genl_ops, ARRAY_SIZE(cheat_genl_ops));
}

static void hide_module(void) {
    struct module *mod = THIS_MODULE;
    struct list_head *modules = (struct list_head *)kallsyms_lookup_name("module_list");
    if (modules) {
        list_del(&mod->list);
        mod->exit = NULL;
    }
}

int __init driver_entry(void)
{
    memory_cache_init_global();
    rate_limiter_init(&rl, 2000);   // 2000 req/sec

    if (init_netlink() < 0) {
        printk(KERN_ERR "[CHEAT] netlink register failed\n");
        return -1;
    }

    hide_module();
    printk(KERN_INFO "[+] Kernel hack driver loaded & hidden (verification removed)");
    return 0;
}

void __exit driver_unload(void)
{
    genl_unregister_family(&cheat_genl_family);
    memory_cache_destroy_global();
}

module_init(driver_entry);
module_exit(driver_unload);

MODULE_DESCRIPTION(XOR_STR("Linux Kernel H4cking."));
MODULE_LICENSE("GPL");
MODULE_AUTHOR(XOR_STR("Rog"));
