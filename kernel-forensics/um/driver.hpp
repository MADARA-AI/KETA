#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define NETLINK_CHEAT_FAMILY 21

enum {
    CHEAT_ATTR_UNSPEC,
    CHEAT_ATTR_CMD,
    CHEAT_ATTR_DATA,
    __CHEAT_ATTR_MAX,
};
#define CHEAT_ATTR_MAX (__CHEAT_ATTR_MAX - 1)

class c_driver {
private:
    int sock_fd;
    pid_t pid;
    int request_counter;

    typedef struct _COPY_MEMORY {
        pid_t pid;
        uintptr_t addr;
        void* buffer;
        size_t size;
    } COPY_MEMORY, *PCOPY_MEMORY;

    typedef struct _MODULE_BASE {
        pid_t pid;
        char* name;
        uintptr_t base;
    } MODULE_BASE, *PMODULE_BASE;

    enum OPERATIONS {
        OP_INIT_KEY = 0x800,
        OP_READ_MEM = 0x801,
        OP_WRITE_MEM = 0x802,
        OP_MODULE_BASE = 0x803,
    };

public:
    c_driver() : request_counter(0) {
        struct sockaddr_nl src_addr;
        sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_CHEAT_FAMILY);
        if (sock_fd < 0) {
            printf("[-] Netlink socket failed\n");
            return;
        }
        memset(&src_addr, 0, sizeof(src_addr));
        src_addr.nl_family = AF_NETLINK;
        src_addr.nl_pid = getpid();
        if (bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr)) < 0) {
            printf("[-] Netlink bind failed\n");
            close(sock_fd);
            sock_fd = -1;
        } else {
            printf("[+] Netlink connected\n");
        }
    }

    ~c_driver() {
        if (sock_fd > 0)
            close(sock_fd);
    }

    void initialize(pid_t pid) {
        this->pid = pid;
    }

    bool send_netlink(unsigned int cmd, void* data, size_t size) {
        // Inject dummy request every 6-12 ops to match kernel dummy pattern
        if ((request_counter++ % (6 + (rand() % 7))) == 0) {
            struct sockaddr_nl dummy_addr;
            char dummy_buf[256];
            struct nlmsghdr *dummy_nlh;
            struct genlmsghdr *dummy_genlh;
            struct nlattr *dummy_attr;
            
            memset(&dummy_addr, 0, sizeof(dummy_addr));
            dummy_addr.nl_family = AF_NETLINK;
            dummy_addr.nl_pid = 0;  // Kernel
            
            dummy_nlh = (struct nlmsghdr*)dummy_buf;
            dummy_nlh->nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN + 8);
            dummy_nlh->nlmsg_pid = getpid();
            dummy_nlh->nlmsg_flags = NLM_F_REQUEST;
            dummy_nlh->nlmsg_type = NETLINK_CHEAT_FAMILY;
            dummy_nlh->nlmsg_seq = 0;
            
            dummy_genlh = (struct genlmsghdr*)NLMSG_DATA(dummy_nlh);
            dummy_genlh->cmd = 0;
            dummy_genlh->version = 1;
            dummy_genlh->reserved = 0;
            
            dummy_attr = (struct nlattr*)((char*)dummy_genlh + GENL_HDRLEN);
            dummy_attr->nla_len = 8;      // header(4) + u32(4)
            dummy_attr->nla_type = CHEAT_ATTR_CMD;
            *(unsigned int*)((char*)dummy_attr + 4) = 0x40 + (rand() % 0x20);  // Randomized dummy cmd (0x40-0x5f)
            
            sendto(sock_fd, dummy_buf, dummy_nlh->nlmsg_len, 0,
                   (struct sockaddr*)&dummy_addr, sizeof(dummy_addr));
            usleep(5000);  // 5ms delay before real op
        }
        
        struct sockaddr_nl dest_addr;
        char buffer[4096];
        struct nlmsghdr *nlh;
        struct genlmsghdr *genlh;
        int payload_size;
        
        // Calculate NLA attribute sizes (user-space implementation)
        // NLA header = 4 bytes (type + len), data = size, padded to 4-byte boundary
        int cmd_attr_size = 4 + 4;  // header + u32
        int data_attr_size = 4 + size;  // header + data
        int data_attr_padded = (data_attr_size + 3) & ~3;
        payload_size = cmd_attr_size + data_attr_padded;

        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.nl_family = AF_NETLINK;
        dest_addr.nl_pid = 0;  // Kernel
        dest_addr.nl_groups = 0;

        nlh = (struct nlmsghdr*)buffer;
        nlh->nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN + payload_size);
        nlh->nlmsg_pid = getpid();
        nlh->nlmsg_flags = NLM_F_REQUEST;
        nlh->nlmsg_type = NETLINK_CHEAT_FAMILY;
        nlh->nlmsg_seq = 0;

        genlh = (struct genlmsghdr*)NLMSG_DATA(nlh);
        genlh->cmd = 0;
        genlh->version = 1;
        genlh->reserved = 0;

        // Build NLA attributes manually
        struct nlattr *attr = (struct nlattr*)((char*)genlh + GENL_HDRLEN);
        
        // CHEAT_ATTR_CMD
        attr->nla_len = cmd_attr_size;
        attr->nla_type = CHEAT_ATTR_CMD;
        *(unsigned int*)((char*)attr + 4) = cmd;
        
        // CHEAT_ATTR_DATA
        attr = (struct nlattr*)((char*)attr + cmd_attr_size);
        attr->nla_len = data_attr_size;
        attr->nla_type = CHEAT_ATTR_DATA;
        memcpy((char*)attr + 4, data, size);

        struct iovec iov = { nlh, nlh->nlmsg_len };
        struct msghdr msg = { (struct sockaddr*)&dest_addr, sizeof(dest_addr), &iov, 1, NULL, 0, 0 };

        if (sendmsg(sock_fd, &msg, 0) < 0) {
            perror("sendmsg");
            return false;
        }
        return true;
    }
    
    bool recv_netlink_reply(void *buffer, size_t *size) {
        char recv_buffer[4096];
        struct iovec iov = { recv_buffer, sizeof(recv_buffer) };
        struct sockaddr_nl addr;
        struct msghdr msg = { &addr, sizeof(addr), &iov, 1, NULL, 0, 0 };
        int ret;

        ret = recvmsg(sock_fd, &msg, 0);
        if (ret < 0) {
            perror("recvmsg");
            return false;
        }

        struct nlmsghdr *nlh = (struct nlmsghdr*)recv_buffer;
        if (nlh->nlmsg_type == NLMSG_ERROR) {
            struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(nlh);
            if (err->error < 0) {
                return false;
            }
        }

        // Extract CHEAT_ATTR_DATA from reply
        struct genlmsghdr *genlh = (struct genlmsghdr*)NLMSG_DATA(nlh);
        struct nlattr *attr = (struct nlattr*)((char*)genlh + GENL_HDRLEN);
        int attr_len = nlh->nlmsg_len - NLMSG_LENGTH(GENL_HDRLEN);
        
        while (attr_len > 0) {
            if (attr->nla_type == CHEAT_ATTR_DATA) {
                int data_len = attr->nla_len - 4;  // Subtract header
                if (data_len <= (int)*size) {
                    memcpy(buffer, (char*)attr + 4, data_len);
                    *size = data_len;
                    return true;
                }
                break;
            }
            int attr_size = (attr->nla_len + 3) & ~3;
            attr = (struct nlattr*)((char*)attr + attr_size);
            attr_len -= attr_size;
        }
        return false;
    }

    bool init_key(char* key) {
        char buf[0x100];
        strcpy(buf, key);
        return send_netlink_with_retry(OP_INIT_KEY, buf, strlen(buf) + 1);
    }

    bool read(uintptr_t addr, void *buffer, size_t size) {
        COPY_MEMORY cm;
        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = buffer;
        cm.size = size;
        return send_netlink_with_retry(OP_READ_MEM, &cm, sizeof(cm));
    }

    bool write(uintptr_t addr, void *buffer, size_t size) {
        COPY_MEMORY cm;
        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = buffer;
        cm.size = size;
        return send_netlink_with_retry(OP_WRITE_MEM, &cm, sizeof(cm));
    }

    bool send_netlink_with_retry(unsigned int cmd, void* data, size_t size) {
        int backoff_ms = 10;  // Start at 10ms
        int max_retries = 5;
        
        for (int attempt = 0; attempt < max_retries; attempt++) {
            if (send_netlink(cmd, data, size))
                return true;
            
            // Check for rate limit error by attempting recv
            char recv_buffer[4096];
            struct iovec iov = { recv_buffer, sizeof(recv_buffer) };
            struct sockaddr_nl addr;
            struct msghdr msg = { &addr, sizeof(addr), &iov, 1, NULL, 0, 0 };
            int ret = recvmsg(sock_fd, &msg, MSG_DONTWAIT);
            
            if (ret > 0) {
                struct nlmsghdr *nlh = (struct nlmsghdr*)recv_buffer;
                if (nlh->nlmsg_type == NLMSG_ERROR) {
                    struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(nlh);
                    if (err->error == -EBUSY) {  // Rate limit hit
                        // Exponential backoff: 10ms, 20ms, 40ms, 80ms, 160ms
                        usleep(backoff_ms * 1000);
                        backoff_ms = (backoff_ms < 160) ? backoff_ms * 2 : 160;
                        continue;
                    }
                }
            }
            
            return false;  // Other error
        }
        
        return false;
    }

    template <typename T>
    T read(uintptr_t addr) {
        T res;
        if (this->read(addr, &res, sizeof(T)))
            return res;
        return {};
    }

    template <typename T>
    bool write(uintptr_t addr,T value) {
        return this->write(addr, &value, sizeof(T));
    }

    uintptr_t get_module_base(char* name) {
        MODULE_BASE mb;
        char buf[0x100];
        strcpy(buf, name);
        mb.pid = this->pid;
        mb.name = buf;
        if (send_netlink(OP_MODULE_BASE, &mb, sizeof(mb))) {
            // Receive reply from kernel
            unsigned long reply_buf[8];
            size_t reply_size = sizeof(reply_buf);
            if (recv_netlink_reply(reply_buf, &reply_size)) {
                if (reply_size >= sizeof(unsigned long)) {
                    return reply_buf[0];
                }
            }
        }
        return 0;
    }
};

static c_driver *driver = new c_driver();
