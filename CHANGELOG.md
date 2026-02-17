# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-02-17

### Added
- **Generic Netlink Communication**: Replaced miscdevice with Generic Netlink (family 29) for stealthier kernel-user interaction. No `/dev/` files, less detectable than traditional ioctl.
- **Per-PID Session-Based Verification**: Implemented spinlock-protected PID array for verification state. Fixes race condition where multiple processes shared verification flag.
- **Module Hiding**: Kernel module unlinks itself from module list via `list_del(&mod->list)` and disables exit handler. Invisible to `lsmod` and `/proc/modules`.
- **String Obfuscation**: XOR-based runtime string decryption for all sensitive strings (module description, author, RC4 key). Uses static buffers (zero-copy).
- **Behavioral Randomization**: Per-page random delays (0.4-2.2ms) during memory operations to evade timing-based detection.
- **Anti-Debug Protection**: Denies access to processes with ptrace flag set via `current->ptrace & PT_PTRACED`.
- **Optimized Memory Caching**: Hash-based cache lookup with linear probing (O(1) best case vs O(n) linear search).
- **Rate Limiting**: Configurable rate limiter to prevent detection via burst access patterns.
- **Netlink Reply Handling**: Proper `genlmsg_reply()` implementation for bi-directional communication.

### Fixed
- **XOR_STR Memory Leak**: Macro now returns static buffer instead of allocating with `kmalloc`. Saves ~100 bytes per string usage.
- **is_verified Race Condition**: Global boolean replaced with per-PID tracking array protected by spinlock. Prevents multiple processes from interfering with each other's verification state.
- **Broken OP_MODULE_BASE**: Kernel now sends module base address back to userspace via `genlmsg_reply()` with NLA attribute.
- **Userspace Compilation Failure**: Removed kernel-only macros (`nla_total_size`, `nla_data`, etc.). Implemented manual NLA attribute parsing in userspace.
- **Incomplete Netlink Reply Reception**: Added `recv_netlink_reply()` function in userspace driver to read kernel responses.
- **Cache Lookup Performance**: Switched from linear O(n) search to hash-based index with linear probing.

### Changed
- Family name obfuscated to `"diag"` (XOR encrypted) instead of obvious `"CHEAT_NETLINK"`.
- Family number changed to 29 (more common on Android) instead of 31.
- Removed `GENL_ADMIN_PERM` flag to allow non-root processes (if needed).
- Improved error handling and validation in Netlink message parsing.

### Security
- ⚠️ **Known Limitation**: XOR obfuscation with fixed key 0xAA is weak. Easily recovered via string analysis.
- ⚠️ **Known Limitation**: Netlink traffic is visible in `/proc/net/netlink` for forensic analysis.
- ⚠️ **Known Limitation**: No syscall hooking (getdents64, etc.) for hiding processes/files.
- Proper size validation for NLA payloads to prevent buffer overflows.
- Spinlock protection for all shared state (verification, cache).

### Known Issues
- Netlink family conflict possible on systems with custom netlink families.
- Module cannot be unloaded if exit disabled (by design for persistence).
- Timeout handling for Netlink replies not implemented (may block indefinitely).
- No authentication on Netlink messages (any process can send commands if verified).

### Documentation
- Added CHANGELOG.md
- Added BUILDING.md with detailed compilation instructions
- Added SECURITY.md with legal disclaimers and detection vectors
- Updated README.md with current feature list and recent fixes

---

## [Unreleased]

### Planned for v1.1
- [ ] AES encryption instead of XOR (requires crypto libraries)
- [ ] Syscall hooking via ftrace for getdents64 (hide processes)
- [ ] Netlink message authentication (HMAC-based)
- [ ] Memory encryption for sensitive data
- [ ] Timeout handling for Netlink operations
- [ ] Better verification scheme (challenge-response instead of fixed hashes)
- [ ] Support for older Android kernels (< 5.0)
