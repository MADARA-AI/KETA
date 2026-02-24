# Linux Kernel Memory Access & Forensics Module

> Generic Netlink-based kernel module for memory forensics with anti-detection hardening

[![Status](https://img.shields.io/badge/status-research-blue)]() [![Platform](https://img.shields.io/badge/platform-Linux%20ARM64-lightgrey)]() [![License](https://img.shields.io/badge/license-research%20only-red)]()

## Overview

A Linux kernel module for memory forensics and debugging. Enables direct physical memory read/write operations through page table walking with Generic Netlink communication and multi-phase obfuscation.

**Key Features:**
- Direct physical memory R/W via page table translation
- Module base address resolution for loaded libraries
- Per-device authentication with fingerprint binding
- Generic Netlink communication (no `/dev/` files)
- Rate limiting and traffic pattern randomization
- 5-phase behavioral obfuscation

## Quick Start

### Prerequisites
- Linux ARM64/AArch64 (Android 4.9+)
- Root access
- Build tools: `make`, `gcc`/`clang`, kernel headers

### Installation

```bash
# Build and load kernel module
cd kernel-forensics/km && make
sudo insmod diag_helper.ko

# Build userspace client  
cd ../um && make
```

### Basic Usage

```bash
# Device verification (once per boot)
sudo ./main 12345 verify "$(uuidgen)"

# Read process memory
sudo ./main 12345 read 0x7ffabcd000 256

# Write data to memory
sudo ./main 12345 write 0x7ffabcd000 "41424344" 4

# Resolve library base address
sudo ./main 12345 base libcore.so
```

## Architecture

```
kernel-forensics/
├── km/          # Kernel module (385 LOC)
│   ├── entry.c      - Netlink handlers
│   ├── memory.c     - Page table walking
│   ├── verify.c     - Authentication
│   └── rate_limit.h - Request throttling
└── um/          # Userspace client (298 LOC)  
    ├── driver.hpp   - Netlink communication
    └── main.cpp     - CLI interface
```

### Obfuscation Phases

| Phase | Features |
|-------|----------|
| 1 | Netlink mimicry, jitter, symbol reduction |
| 2 | Rate limiting, device binding |
| 3 | Thermal traffic, lazy registration |
| 4 | Randomized parameters |
| 4.5 | Dynamic commands, error injection |

### Performance

- Binary Size: 45 KB (stripped)
- CPU/Memory Overhead: <1%
- Operation Rate: 400 req/sec
- Protocol: Generic Netlink
- Exported Symbols: <50 (75% reduction)

## Technical Details

**Memory Access:**
- Page table walking via `follow_phys()`
- Virtual → physical address translation
- Physical memory access via `ioremap()` (bypasses MMU)
- Multi-page handling with adaptive jitter

**Authentication:**
- Device-specific 32-character key binding
- Per-device fingerprinting
- Token bucket rate limiter
- Exponential backoff (10-160ms)

**Anti-Detection:**
- Legitimate Netlink family (21)
- Randomized operation counts
- Dummy thermal replies
- 75% symbol reduction
- Dynamic command range per boot
- List unlinking and sysfs cleanup

## Build Requirements

- **Kernel:** Linux 4.9+ (ARM64/AArch64)
- **Tools:** `make`, `gcc`/`clang`, kernel headers
- **Permissions:** Root access
- **Config:** `CONFIG_NETLINK` enabled

## Contributing

1. Review source code architecture
2. Read code comments for implementation details
3. Test changes thoroughly
4. Target ARM64 Linux 4.9+ compatibility

## License & Legal

See LICENSE file for licensing terms.

**Intended Use:**
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
