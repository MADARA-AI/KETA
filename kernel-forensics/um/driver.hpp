#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

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

#define OP_INIT_KEY     0x800
#define OP_READ_MEM     0x801
#define OP_WRITE_MEM    0x802
#define OP_MODULE_BASE  0x803

class c_driver {
private:
    int fd = -1;
    pid_t pid;

public:
    c_driver() {
        // Scan /dev/ for diagXXXXXXXX device
        DIR *dir = opendir("/dev");
        if (!dir) {
            printf("[-] Cannot open /dev\n");
            return;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "diag", 4) == 0 && strlen(entry->d_name) == 12) {
                char path[64];
                snprintf(path, sizeof(path), "/dev/%s", entry->d_name);
                fd = open(path, O_RDWR);
                if (fd > 0) {
                    printf("[+] Connected to %s\n", path);
                    break;
                }
            }
        }
        closedir(dir);

        if (fd < 0) {
            printf("[-] Cannot find device. Check dmesg after insmod\n");
        }
    }

    ~c_driver() { if (fd > 0) close(fd); }

    void initialize(pid_t p) { this->pid = p; }

    bool init_key(char* key) { return true; }   // dummy

    bool read(uintptr_t addr, void *buffer, size_t size) {
        if (fd < 0) return false;
        COPY_MEMORY cm = {pid, addr, buffer, size};
        return ioctl(fd, OP_READ_MEM, &cm) == 0;
    }

    bool write(uintptr_t addr, void *buffer, size_t size) {
        if (fd < 0) return false;
        COPY_MEMORY cm = {pid, addr, buffer, size};
        return ioctl(fd, OP_WRITE_MEM, &cm) == 0;
    }

    template <typename T> T read(uintptr_t addr) {
        T res = {};
        read(addr, &res, sizeof(T));
        return res;
    }

    template <typename T> bool write(uintptr_t addr, T value) {
        return write(addr, &value, sizeof(T));
    }

    uintptr_t get_module_base(char* name) {
        if (fd < 0) return 0;
        MODULE_BASE mb = {pid, name, 0};
        if (ioctl(fd, OP_MODULE_BASE, &mb) == 0)
            return mb.base;
        return 0;
    }
};

static c_driver *driver = new c_driver();
