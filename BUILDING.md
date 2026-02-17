# Building kernel_hack

This document provides detailed instructions for building the kernel module and userspace test client.

## Table of Contents
1. [Prerequisites](#prerequisites)
2. [Linux Build](#linux-build)
3. [Android Build](#android-build)
4. [Troubleshooting](#troubleshooting)
5. [Testing](#testing)

---

## Prerequisites

### Common Requirements
- **Kernel headers** matching your target kernel version (5.4+)
- **Build tools**: `make`, `gcc`/`g++` or `clang`
- **Development libraries**: `linux-headers` package

### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install build-essential linux-headers-$(uname -r)
```

### Linux (Fedora/RHEL)
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install kernel-devel
```

### Android (NDK)
- Android NDK r23+ (or latest)
- Target kernel headers for your device
- Cross-compiler: `aarch64-linux-android-gcc`

Download from: https://developer.android.com/ndk/downloads

```bash
export NDK_ROOT=/path/to/android-ndk-r23c
export PATH=$NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH
```

---

## Linux Build

### Kernel Module

```bash
cd kernel_hack/km

# For native build
make

# For cross-compilation (example: ARM64)
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-

# Clean build artifacts
make clean

# View Makefile options
cat Makefile
```

**Output**: `kernel_hack/km/module.ko` (or name specified in Makefile)

### Userspace Client

```bash
cd kernel_hack/um

# Build
make

# Clean
make clean

# Output: kernel_hack/um/main
```

### Complete Linux Build Script

```bash
#!/bin/bash
set -e

echo "[*] Building kernel module..."
cd kernel_hack/km
make clean
make
echo "[+] Module built: $(pwd)/module.ko"

echo "[*] Building userspace client..."
cd ../um
make clean
make
echo "[+] Client built: $(pwd)/main"

echo "[+] Build complete!"
ls -lh ../km/module.ko ./main
```

---

## Android Build

### Prerequisites
```bash
export ANDROID_NDK=/path/to/android-ndk-r23c
export ARCH=arm64
export CROSS_COMPILE=$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android-
export CC=$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android21-clang
export CXX=$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android21-clang++
```

### Kernel Module

Get kernel headers for your target device:

```bash
# If you have boot.img or kernel source
KERNEL_SRC=/path/to/android-kernel-source

make -C $KERNEL_SRC M=$(pwd)/kernel_hack/km modules CROSS_COMPILE=$CROSS_COMPILE ARCH=arm64
```

Or use generic Linux headers:

```bash
cd kernel_hack/km
make ARCH=arm64 CROSS_COMPILE=$CROSS_COMPILE
```

### Userspace Client

```bash
cd kernel_hack/um
$CXX -O2 -c main.cpp -o main.o
$CXX main.o -o main -lpthread
```

### Push to Android Device

```bash
adb shell su -c "mount -o remount,rw /system"
adb push kernel_hack/km/module.ko /data/local/tmp/
adb push kernel_hack/um/main /data/local/tmp/

# Or use magisk module (advanced)
adb shell su -c "cp /data/local/tmp/module.ko /system/lib/modules/"
```

---

## Building for Multiple Architectures

### ARM64 (Most Android devices)
```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
```

### ARM32 (Older Android)
```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
```

### x86_64 (Intel Android emulator / Linux)
```bash
make ARCH=x86 CROSS_COMPILE=x86_64-linux-gnu-
```

---

## Troubleshooting

### Error: "Cannot find kernel headers"

```bash
# Install kernel headers
sudo apt-get install linux-headers-$(uname -r)

# Or specify headers path
make KDIR=/usr/src/linux-headers-$(uname -r)
```

### Error: "Cross compiler not found"

```bash
# Verify cross-compiler is in PATH
aarch64-linux-gnu-gcc --version

# Or set explicitly
export CC=/path/to/aarch64-linux-gnu-gcc
export CXX=/path/to/aarch64-linux-gnu-g++
```

### Error: "undefined reference to `kallsyms_lookup_name`"

This is expected on some kernels. The module hides itself via `list_del`, which may not work on all kernel versions.

```bash
# Try fallback: Use hardcoded module_list address (risky!)
# Edit entry.c and replace kallsyms_lookup_name call
```

### Error: "Permission denied" when inserting module

```bash
# Need root
sudo insmod kernel_hack/km/module.ko

# Or on Android
adb shell su -c "insmod /data/local/tmp/module.ko"
```

### Compilation Warning: "comparison of unsigned expression < 0"

This is a known issue in older compilers. It's safe to ignore:

```c
// In memory.c
if (sz < 0) { ... }  // sz is ssize_t, which is signed
```

---

## Testing

### Load Module

```bash
sudo insmod kernel_hack/km/module.ko

# Verify it's not in lsmod
lsmod | grep -i cheat
# Should return nothing (module hidden!)

# Check dmesg for logs
dmesg | tail -20
# Should show "[+] Module hidden"
```

### Run Userspace Client

```bash
# First initialize the key
cd kernel_hack/um
./main

# Should print:
# pid = <target_pid>
# base = <hex_address>
# Read 1 times cost = <ms>
# result = <hex_value>
```

### Advanced Testing

```bash
# Monitor kernel messages in real-time
sudo dmesg -w

# In another terminal:
./main

# Check netlink traffic
cat /proc/net/netlink | grep "29"  # Family 29

# Use strace to see syscalls
strace ./main

# Verify module is truly hidden
cat /proc/modules  # No entry for our module
```

### Cleanup

```bash
# Remove module (if possible)
sudo rmmod module_name

# Clean all artifacts
cd kernel_hack/km && make clean
cd ../um && make clean
```

---

## Build Environment Examples

### Docker (Ubuntu 20.04 + Linux Kernel Dev)

```dockerfile
FROM ubuntu:20.04
RUN apt-get update && apt-get install -y \
    build-essential \
    linux-headers-generic \
    git \
    wget

WORKDIR /root
COPY . kernel_hack/

CMD cd kernel_hack/km && make
```

### GitHub Actions CI

```yaml
name: Build

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential linux-headers-generic
      - name: Build kernel module
        run: cd kernel_hack/km && make
      - name: Build userspace
        run: cd kernel_hack/um && make
```

---

## Optimization Flags

For production builds, consider:

```bash
# Kernel module
make CFLAGS="-O2 -fomit-frame-pointer" -j$(nproc)

# Userspace
g++ -O3 -march=native -flto -o main main.cpp
```

---

## Support

For build issues:
1. Check kernel version: `uname -r`
2. Verify headers: `ls /usr/src/linux-headers-$(uname -r)`
3. Check compiler: `gcc --version`
4. See [SECURITY.md](SECURITY.md) for kernel-specific requirements
