# Linux Kernel Memory Access & Forensics Module (2026 Stealth Edition)

> Ioctl-based kernel module for memory forensics with maximum stealth and anti-detection

[![Status](https://img.shields.io/badge/status-stealth-green)]() [![Platform](https://img.shields.io/badge/platform-Linux%20ARM64-lightgrey)]() [![License](https://img.shields.io/badge/license-research%20only-red)]()

## Overview

A stealthy Linux kernel module for memory forensics and debugging. Features **ioctl communication** (not netlink), **full module hiding**, and **randomized device names** for maximum operational security.

**Key Features:**
- ✅ **Ioctl communication** (no netlink traffic - harder to detect)
- ✅ **Random device name** every load (`/dev/diag12345678`)
- ✅ **Full module hiding** (list_del + kobject + sysfs cleanup)
- ✅ **No verification** required (instant access)
- ✅ **Direct physical memory** R/W via page table walking
- ✅ **Module base resolution** for loaded libraries
- ✅ **Rate limiting** (120 req/sec for safety)

## Quick Start

### Prerequisites
- Linux ARM64/AArch64 (Android 4.9+)
- Root access
- Build tools: `make`, `gcc`/`clang`, kernel headers

### Installation

```bash
# Build kernel module
cd kernel-forensics/km
make

# Load module (check dmesg for device name)
sudo insmod diag_helper.ko
sudo dmesg | tail -5
# Look for: [+] Stealth device ready → /dev/diagXXXXXXXX

# Build userspace client  
cd ../um
make
```

### Basic Usage

```cpp
#include "driver.hpp"

int main() {
    pid_t target_pid = 12345;
    
    // Auto-discovers /dev/diagXXXXXXXX
    driver->initialize(target_pid);
    
    // Read memory
    uint64_t value = driver->read<uint64_t>(0x7ffabcd000);
    
    // Write memory
    driver->write<uint32_t>(0x7ffabcd000, 0xdeadbeef);
    
    // Get module base
    uintptr_t base = driver->get_module_base("libunity.so");
    printf("Base: 0x%lx\n", base);
    
    return 0;
}
```

## Architecture

```
kernel-forensics/
├── km/          # Kernel module (~150 LOC - simplified)
│   ├── entry.c      - Ioctl handlers + full stealth hiding
│   ├── memory.c     - Page table walking (unchanged)
│   ├── process.c    - Module base resolution (unchanged)
│   └── rate_limit.h - Request throttling (120 req/sec)
└── um/          # Userspace client (~100 LOC - cleaner)
    ├── driver.hpp   - Ioctl communication + auto-discovery
    └── main.cpp     - Example usage
```

### 2026 Stealth Features

| Feature | Description |
|---------|-------------|
| **Ioctl comm** | Direct file operations - no netlink traffic patterns |
| **Random device** | `/dev/diag` + 8 random hex digits (changes every load) |
| **Full hiding** | `list_del()` + name wipe + `kobject_del()` + sysfs cleanup |
| **Auto-discovery** | Client scans `/dev/` for `diagXXXXXXXX` pattern |
| **No verification** | No keys, no auth - instant access |
| **Low profile** | 120 req/sec limit (vs 2000+ in old netlink) |

### Performance

- Binary Size: ~30 KB (stripped, 40% smaller than netlink)
- CPU Overhead: <0.5%
- Operation Rate: 120 req/sec (safe limit)
- Communication: Ioctl (file_operations)
- Visibility: Hidden from `lsmod`, `/proc/modules`, `/sys/module`

## Technical Details

**Memory Access:**
- Page table walking via `follow_phys()`
- Virtual → physical address translation
- Physical memory access vicopy_to_user/copy_from_user

**Communication Protocol:**
- **Ioctl-based**: Standard file operations (`unlocked_ioctl`)
- **Commands**: `OP_READ_MEM`, `OP_WRITE_MEM`, `OP_MODULE_BASE`
- **Device**: `/dev/diagXXXXXXXX` (8 random hex digits)
- **Auto-discovery**: Client scans `/dev/` directory on startup

**Stealth Mechanisms:**
- `list_del(&mod->list)` - Remove from module list (hides from `lsmod`)
- `mod->name[0] = '\0'` - Wipe module name string
- `kobject_del(&mod->mkobj.kobj)` - Remove from sysfs
- `kobject_del(mod->holders_dir)` - Clean up holders directory
- Random device name - Different every load (no fixed `/dev/cheat`)
- No netlink traffic - Ioctl looks like normal device I/O

**Rate Limiting:**
- Token bucket algorithm
- 120 requests/second limit
- Prevents detection via traffic analysis
- Returns `-EBUSY` when limit exceeded
- List unlinking and sysfs cleanup
MISC_DEVICES` enabled (standard)

## Operational Security

**Detection Resistance:**
- ✅ No netlink traffic (ioctl looks like normal device I/O)
- ✅ Hidden from `lsmod`, `/proc/modules`, `/sys/module/*`
- ✅ Random device name (different every boot)
- ✅ No static signatures or strings
- ✅ Low request rate (120/sec = normal device usage)

**Usage Tips:**
- Check `dmesg` after `insmod` to find exact device name
- Module remains hidden even after loading
- Client auto-discovers device (no hardcoded paths)
- Keep request rate below 100/sec for max stealth
## Build Requirements

- **Kernel:** Linux 4.9+ (ARM64/AArch64)
- **Tools:** `make`, `gcc`/`clang`, kernel headers
- **Permissions:** Root access
- **Config:** `CONFIG_NETLINK` enabled

## Contributing

1. Review source code architecture
2. Read code comments for implementation details
3. Test changes thoroughly you own
- ⚠️ Comply with applicable laws and regulations
- ⚠️ Not for bypassing anti-cheat or DRM systems
- ⚠️ Requires proper disclosure for security research
- ⚠️ Hidden modules can persist until reboot

---

**Version:** 2.0 (2026 Stealth Edition) | **Type:** Research & Security Analysis | **Platform:** Linux ARM64
- ✓ Kernel development and debugging
- ✓ Memory forensics and training
- ✓ Security research and assessment
- ✓ Anti-forensics analysis
- ✓ Kernel visibility study

**Important:**
- ⚠️ Only use on authorized systems
- ⚠️ Comply with applicable laws
- ⚠️ Not for circumventing anti-cheat systems
- ⚠️ Requires proper disclosure for research

---

**Type:** Research & Security Analysis | **Platform:** Linux ARM64 | **Status:** Production-Ready
