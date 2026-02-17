# Security, Legal, & Ethical Disclaimer

## ⚠️ CRITICAL: Read Before Using

This project is **for educational and research purposes only**. It demonstrates kernel-level anti-detection techniques used in advanced rootkits and game cheats. 

### Legal Notice

**Using this code to:**
- Cheat in online games (PUBG, Valorant, Fortnite, etc.)
- Bypass anti-cheat systems (EAC, BattlEye, Vanguard, etc.)
- Circumvent security on devices you don't own
- Violate Terms of Service

**Is ILLEGAL and may result in:**
- ❌ Game bans (permanent)
- ❌ Civil lawsuits from game publishers
- ❌ Criminal charges (Computer Fraud & Abuse Act in US, similar laws elsewhere)
- ❌ Device bans/bricking
- ❌ ISP/network termination

**Only use on:**
- ✅ VMs you own
- ✅ Test devices (disposable/old phones)
- ✅ Your own personal Linux systems
- ✅ Educational/research contexts with proper disclosure

---

## What This Code Does

This kernel module provides:
- Direct access to process memory from kernel space
- Module hiding from system utilities
- Netlink-based command interface
- Behavioral randomization to evade detection

This is equivalent to a **Level-1 Rootkit**. It can:
- Read/write arbitrary process memory
- Hide its own presence from userspace tools
- Operate without direct `/dev/` interface

---

## What This Code DOES NOT Do

❌ It does **NOT** automatically bypass PUBG, Valorant, or other anti-cheats.
❌ It does **NOT** guarantee undetectability (anti-cheats evolve constantly).
❌ It does **NOT** handle kernel attestation (Secure Boot, ARM TrustZone).
❌ It does **NOT** include syscall hooking or filesystem hiding.
❌ It does **NOT** provide legal protection if misused.

---

## Detection Vectors: What Evades & What Doesn't

### ✅ Evaded by This Code
| Detection Method | How Evaded |
|---|---|
| `lsmod` | Module unhooks from module list |
| `/proc/modules` | Module not in list |
| Device file scanning | No `/dev/` file (uses Netlink) |
| Static string analysis | XOR obfuscation at runtime |
| Basic timing analysis | Random 0.4-2.2ms jitter per page |
| `dmesg` timestamps | Normal kernel logs |

### ❌ NOT Evaded (Detectable)
| Detection Method | Why |
|---|---|
| `/proc/net/netlink` inspection | Family 29 traffic visible to root |
| Memory scanning (busybox memdump) | Can still find kernel code |
| ftrace/tracepoint hooks | Syscalls still traced |
| Kernel symbol enumeration | kallsyms still contains references |
| CPU/memory anomalies | Delays cause timing patterns |
| Integrity verification (dm-verity) | Kernel changes detected |

---

## Modern Anti-Cheat Analysis (2025-2026)

### PUBG Mobile Anti-Cheat (estimated)
- Scans for unsigned kernel modules
- Checks `/proc/net/netlink` against whitelist
- Hooks memory access syscalls
- Monitors for timing anomalies
- **Verdict**: This code likely detected within hours

### Valorant Vanguard (estimated)
- Kernel-level driver (more privileged than this code)
- Real-time process memory monitoring
- Whitelisted syscall filtering
- **Verdict**: Cannot compete; Vanguard has higher privilege

### BattlEye (estimated)
- Established detection signatures
- Regularly updated detection database
- Active reverse-engineering team
- **Verdict**: Likely detected on first use

### Conclusion
**This code will NOT make you undetectable in 2026.** Modern anti-cheats are kernel-level or higher. Expect bans within 1-24 hours of using this.

---

## Kernel-Level Requirements

### Supported Kernels
- Linux 5.4+ (tested focus)
- Android 10+ (kernel version varies by OEM)

### Features Dependent on Kernel Version
| Feature | Min Version | Notes |
|---|---|---|
| Generic Netlink | 4.0+ | Standard kernel feature |
| Module hiding | 4.0+ | `list_del` works |
| kallsyms_lookup_name | 4.0+ | May be restricted on newer kernels |
| `current->ptrace` | 2.6+ | Anti-debug check |

### Kernel Configuration
For building on your kernel:
```bash
# Check if Netlink is enabled
grep CONFIG_NETLINK /boot/config-$(uname -r)
# Should output: CONFIG_NETLINK=y

# Check if kernel is not hardened
grep CONFIG_HARDENED_USERCOPY /boot/config-$(uname -r)
# If CONFIG_HARDENED_USERCOPY=y, some features may fail

# Check kernel lockdown status
grep CONFIG_SECURITY_LOCKDOWN /boot/config-$(uname -r)
```

---

## Security Hardening Against This Code

### For System Administrators
1. **Enable Secure Boot**: Prevent unsigned kernel modules
2. **Use SELinux/AppArmor**: Restrict Netlink usage
3. **Enable dm-verity**: Detect kernel modifications
4. **Disable CONFIG_KALLSYMS**: Prevent symbol enumeration
5. **Use LSM (Linux Security Modules)**: Monitor syscalls

### For Device Manufacturers
1. Implement **kernel attestation** (Measured Boot, TPM)
2. Use **ARM TrustZone** for security-critical operations
3. Lock bootloader with signed firmware
4. Enable **SMACK** or **SELinux** mandatory policies

---

## Code Quality & Safety Notes

### This Code Is:
- ✅ Well-commented and documented
- ✅ Includes error handling for most cases
- ✅ Uses proper locking (spinlocks, semaphores)
- ✅ Fixed critical memory leaks and race conditions

### This Code Is NOT:
- ❌ Production-ready (no attestation, no hardening)
- ❌ Optimized for stealth against modern anti-cheats
- ❌ Resistant to forensic analysis
- ❌ Guaranteed to work on all kernel versions

### Known Security Weaknesses
1. **XOR obfuscation with fixed key** is trivially breakable
   - Recovery: `strings kernel.ko | xxd | head`
   
2. **Netlink family number 29** is hardcoded and findable
   - Better: Randomize at compile-time or runtime
   
3. **Per-PID state array** has no authentication
   - Better: Use HMAC or session tokens
   
4. **No syscall hooking** means processes are still visible
   - Better: Implement getdents64 hook + ftrace

---

## Responsible Disclosure

If you discover a security vulnerability in this code:
1. **Do not post publicly** until a fix is available
2. Email security details to: [contact if added to repo]
3. Include: vulnerability description, kernel version, impact
4. Allow 30 days for fix before public disclosure

---

## Educational Use Guidelines

If using this for education/research:

### Recommended Contexts
- ✅ University kernel development courses
- ✅ Advanced OS security seminars
- ✅ Cybersecurity capture-the-flag (CTF) competitions
- ✅ Authorized penetration testing (with written permission)
- ✅ Self-hosted labs on your own hardware

### Suggested Study Topics
1. Kernel module development (LKM basics)
2. Memory protection & paging (MMU/TLB)
3. Netlink protocol design
4. Anti-detection/stealth techniques
5. Race condition prevention (spinlocks, atomics)
6. Rootkit architecture & design

### Classroom Disclaimer Template
If teaching with this code, include:

```
[PROFESSOR DISCLAIMER]
This code is used for educational purposes to understand 
kernel-level attacks and defenses. Students are strictly 
prohibited from using this outside the lab environment 
for any illegal activity. Violations will result in 
disciplinary action and potential criminal charges.
```

---

## Alternative: Ethical Learning Resources

If you want to learn kernel security without risking legality:

### Legitimate Resources
- Linux kernel documentation: https://www.kernel.org/doc/
- OWASP kernel security guide
- ARM TrustZone documentation
- Xen hypervisor security design
- SELinux policy development
- Linux kernel module tutorial (LDD3)

### Challenges & Competitions
- CSAW CTF (has kernel category)
- PlaidCTF (reversing + exploitation)
- USENIX Security conference (research)

---

## Checklist Before Use

- [ ] I own or have permission to use this device
- [ ] I understand the legal risks in my jurisdiction
- [ ] I will NOT use this against online games or services
- [ ] I will NOT distribute this for malicious purposes
- [ ] I am using this for legitimate education/research
- [ ] I understand this will likely not bypass modern anti-cheats
- [ ] I accept all legal responsibility for my actions

---

## Disclaimer Summary

```
THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, 
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, 
AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR 
ANY CLAIM, DAMAGES, OR OTHER LIABILITY.

BY USING THIS CODE, YOU AGREE THAT:
1. YOU ASSUME ALL LEGAL AND ETHICAL RESPONSIBILITY
2. THE AUTHOR IS NOT LIABLE FOR MISUSE OR ILLEGAL ACTIVITY
3. YOU WILL NOT USE THIS FOR CHEATING OR UNAUTHORIZED ACCESS
4. YOU UNDERSTAND THE RISKS OF KERNEL-LEVEL CODE
```

---

## Contact & Support

For legitimate questions about:
- Kernel development: See Linux Kernel Mailing List
- Security research: See academic papers and conferences
- This specific code: Refer to README.md and BUILDING.md

**Do not email with requests for help cheating in games.**

---

**Last Updated**: 2026-02-17
**Author**: kernel_hack contributors
**Jurisdiction**: Educational code - user assumes all legal responsibility
