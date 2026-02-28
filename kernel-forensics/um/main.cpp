#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "driver.hpp"

pid_t get_name_pid(char* name) {
    FILE* fp;
    pid_t pid;
    char cmd[0x100] = "pidof ";

    strcat(cmd, name);
    fp = popen(cmd, "r");
    fscanf(fp, "%d", &pid);
    pclose(fp);
    return pid;
}

int main() {
    pid_t pid = get_name_pid((char*)"com.tencent.ig");
    printf("PUBG PID = %d\n", pid);

    // driver->init_key("your_key_here");   // no longer needed
    driver->initialize(pid);

    uintptr_t base = driver->get_module_base("libUE4.so");
    if (base == 0) {
        printf("[-] Failed to get libUE4 base\n");
    } else {
        printf("[+] libUE4.so base = 0x%lx\n", base);
    }

    return 0;
}