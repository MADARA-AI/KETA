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

#define MAX_TRANSFER_SIZE (PAGE_SIZE * 16)  // 64KB limit
#define XOR_KEY 0xAA
#define XOR_STR(str) ({ \
    static char buf[sizeof(str)]; \
    static bool init = false; \
    if (!init) { \
        strcpy(buf, str); \
        for (int i = 0; buf[i]; i++) buf[i] ^= XOR_KEY; \
        init = true; \
    } \
    buf;  /* Return static buffer, no allocation */ \
})

#define NETLINK_CHEAT_FAMILY 21
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
    .name       = XOR_STR("nl_diag"),
    .version    = 1,
    .maxattr    = CHEAT_ATTR_MAX,
    .policy     = cheat_genl_policy,
    .module     = THIS_MODULE,
    .parallel_ops = true,
};

static struct rate_limiter rl;

// Lazy netlink registration
static bool family_registered = false;
static DEFINE_SPINLOCK(family_lock);

// Per-PID verification state to avoid race conditions
#define VERIFIED_PIDS_MAX 32
static pid_t verified_pids[VERIFIED_PIDS_MAX] = {0};
static DEFINE_SPINLOCK(verified_lock);

// Dummy reply counter for traffic obfuscation
static atomic_t dummy_counter = ATOMIC_INIT(0);

// Dynamic dummy command (randomized per boot to defeat fingerprinting)
static u32 dummy_cmd = 0;

// Randomized thermal baseline (varies per boot, harder to fingerprint globally)
static float temp_bias = 0.0;

// Fake thermal module parameters for legitimacy
static int fake_thermal_temp = 42;
static int fake_power_state = 1;
module_param_named(temp, fake_thermal_temp, int, 0444);
module_param_named(power_ok, fake_power_state, int, 0444);
MODULE_PARM_DESC(temp, "Current thermal zone temperature (C)");
MODULE_PARM_DESC(power_ok, "Power supply status (1=OK)");

static bool is_pid_verified(pid_t pid) {
    int i;
    bool found = false;
    spin_lock(&verified_lock);
    for (i = 0; i < VERIFIED_PIDS_MAX; i++) {
        if (verified_pids[i] == pid) {
            found = true;
            break;
        }
    }
    spin_unlock(&verified_lock);
    return found;
}

static void mark_pid_verified(pid_t pid) {
    int i;
    spin_lock(&verified_lock);
    for (i = 0; i < VERIFIED_PIDS_MAX; i++) {
        if (verified_pids[i] == 0) {
            verified_pids[i] = pid;
            break;
        }
    }
    spin_unlock(&verified_lock);
}

// Send dummy thermal-like reply to obfuscate traffic pattern
static int send_dummy_reply(struct genl_info *info) {
    struct sk_buff *reply;
    void *hdr;
    
    // 10% chance to return fake error for realism (EAGAIN mimics rate limit)
    if ((get_random_u32() % 10) == 0) {
        return -EAGAIN;  // Client sees normal retry behavior
    }
    
    reply = genlmsg_new(256, GFP_KERNEL);
    if (!reply) return -ENOMEM;
    
    hdr = genlmsg_put(reply, info->snd_portid, info->snd_seq,
                      &cheat_genl_family, 0, dummy_cmd);  // Dynamic dummy cmd (0x40-0x5f)
    if (!hdr) {
        nlmsg_free(reply);
        return -EMSGSIZE;
    }
    
    // Add randomized thermal data to avoid fingerprinting
    char dummy_str[64];
    snprintf(dummy_str, sizeof(dummy_str),
             "temp=%.1fC:pwr=%s:cpu=%.0f%%",
             temp_bias + (get_random_u32() % 150) / 10.0,   // Vary around boot-time baseline
             (get_random_u32() % 100 < 95) ? "OK" : "WARN",
             10.0 + (get_random_u32() % 800) / 10.0);  // 10-90% CPU
    nla_put_string(reply, CHEAT_ATTR_DATA, dummy_str);
    genlmsg_end(reply, hdr);
    
    return genlmsg_unicast(&init_net, reply, info->snd_portid);
}

static int cheat_do_operation(struct sk_buff *skb, struct genl_info *info)
{
    u32 cmd;
    struct nlattr *data_attr;
    void *data;
    size_t len;
    COPY_MEMORY cm;
    MODULE_BASE mb;
    char key[0x100] = {0};
    char name[0x100] = {0};
    pid_t caller_pid = current->tgid;
    u32 random_delay;
    void *kbuf = NULL;
    int ret = -EINVAL;
    
    // Obfuscate traffic: send dummy reply every 5-13 ops (randomized interval)
    if ((atomic_inc_return(&dummy_counter) % (5 + (get_random_u32() % 9))) == 0) {
        send_dummy_reply(info);
        return 0;  // Eat the request and send dummy
    }

    // Anti-debug
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

    if (cmd == OP_INIT_KEY && !is_verified) {
        if (len >= sizeof(key)) {
            memcpy(key, data, sizeof(key) - 1);
            key[sizeof(key) - 1] = '\0';
        } else {
            memcpy(key, data, len);
            key[len] = '\0';
        }
        if (verify_key_offline(key, strnlen(key, sizeof(key)))) {
            // First successful verification: trigger lazy netlink registration
            if (!family_registered) {
                int reg_ret = lazy_register_netlink();
                if (reg_ret < 0) {
                    return -EAGAIN;  // Registration failed, client should retry
                }
            }
            mark_pid_verified(caller_pid);
            return 0;
        }
        return -EACCES;
    }
    if (!is_pid_verified(caller_pid)) {
        return -EACCES;
    }
    switch (cmd) {
        case OP_READ_MEM:
            {
                if (len < sizeof(cm)) return -EINVAL;
                memcpy(&cm, data, sizeof(cm));
                if (cm.pid <= 0 || cm.size == 0 || cm.size > MAX_TRANSFER_SIZE || cm.addr == 0) {
                    return -EINVAL;
                }
                if (!cm.buffer || !access_ok(cm.buffer, cm.size)) {
                    return -EFAULT;
                }
                kbuf = kmalloc(cm.size, GFP_KERNEL);
                if (!kbuf) {
                    return -ENOMEM;
                }
                if (!read_process_memory(cm.pid, cm.addr, kbuf, cm.size)) {
                    ret = -EIO;
                    goto cleanup;
                }
                if (copy_to_user(cm.buffer, kbuf, cm.size) != 0) {
                    ret = -EFAULT;
                    goto cleanup;
                }
                ret = 0;
            }
            break;
        case OP_WRITE_MEM:
            {
                if (len < sizeof(cm)) return -EINVAL;
                memcpy(&cm, data, sizeof(cm));
                if (cm.pid <= 0 || cm.size == 0 || cm.size > MAX_TRANSFER_SIZE || cm.addr == 0) {
                    return -EINVAL;
                }
                if (!cm.buffer || !access_ok(cm.buffer, cm.size)) {
                    return -EFAULT;
                }
                kbuf = kmalloc(cm.size, GFP_KERNEL);
                if (!kbuf) {
                    return -ENOMEM;
                }
                if (copy_from_user(kbuf, cm.buffer, cm.size) != 0) {
                    ret = -EFAULT;
                    goto cleanup;
                }
                if (!write_process_memory(cm.pid, cm.addr, kbuf, cm.size)) {
                    ret = -EIO;
                    goto cleanup;
                }
                ret = 0;
            }
            break;
        case OP_MODULE_BASE:
            {
                if (len < sizeof(mb)) return -EINVAL;
                memcpy(&mb, data, sizeof(mb));
                if (mb.pid <= 0) {
                    return -EINVAL;
                }
                if (!mb.name || !access_ok(mb.name, 1)) {
                    return -EFAULT;
                }
                if (copy_from_user(name, (void __user*)mb.name, sizeof(name) - 1) != 0) {
                    return -EFAULT;
                }
                name[sizeof(name) - 1] = '\0';
                mb.base = get_module_base(mb.pid, name);
                
                // Send reply via genlmsg_reply with module base
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
                ret = genlmsg_reply(reply, info);
            }
            break;
        default:
            return -ENOTTY;
    }

cleanup:
    if (kbuf) {
        kfree(kbuf);
    }
    return ret;
}

static struct genl_ops cheat_genl_ops[] = {
    {
        .cmd    = 0,
        .doit   = cheat_do_operation,
        .flags  = 0,  // Removed GENL_ADMIN_PERM
    },
};

static int init_netlink(void) {
    // Lazy registration: don't register at module load
    // Only register after first successful key verification
    // This reduces early boot visibility for scanners
    return 0;  // Return success but don't register yet
}

static int lazy_register_netlink(void) {
    int ret;
    bool should_register = false;
    
    spin_lock(&family_lock);
    if (!family_registered) {
        family_registered = true;
        should_register = true;
    }
    spin_unlock(&family_lock);
    
    if (should_register) {
        ret = genl_register_family_with_ops(&cheat_genl_family, cheat_genl_ops, ARRAY_SIZE(cheat_genl_ops));
        if (ret < 0) {
            spin_lock(&family_lock);
            family_registered = false;
            spin_unlock(&family_lock);
            return ret;
        }
    }
    
    return 0;
}

static void hide_module(void) {
    struct module *mod = THIS_MODULE;
    struct list_head *modules = (struct list_head *)kallsyms_lookup_name("module_list");
    if (!modules) return;

    list_del(&mod->list);
    mod->exit = NULL;
    
    // Reduce /sys/module/<name> traces
    mod->sect_attrs = NULL;
    mod->notes_attrs = NULL;
    mod->modinfo_attrs = NULL;
}

int __init driver_entry(void)
{
    int ret;
    
    // Initialize dynamic variables at boot time for per-boot randomization
    dummy_cmd = 0x40 + (get_random_u32() % 0x20);  // Randomize dummy cmd (0x40-0x5f range)
    temp_bias = 35.0 + (get_random_u32() % 80) / 10.0;  // Randomize temp baseline (35-43°C base)
    
    memory_cache_init_global();
    rate_limiter_init(&rl, 400);  // Reduced from 2000: legitimate drivers stay <<500 req/sec

    ret = init_netlink();
    if (ret < 0) {
        printk(KERN_ERR "[CHEAT] netlink register failed\n");
        return ret;
    }
    hide_module();
    printk("[+] Module hidden");
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
