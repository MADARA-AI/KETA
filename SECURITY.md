# Security, Legal, & Ethical Disclaimer

## ⚠️ CRITICAL – READ BEFORE USING OR DISTRIBUTING

This project is **strictly for educational purposes, authorized security research, memory forensics training, and kernel development experimentation**. It demonstrates advanced kernel-level memory access and behavioral obfuscation techniques.

### Explicit Legal & Ethical Restrictions

**Any use of this code to:**
- Cheat, modify, or gain unfair advantage in online games
- Bypass, circumvent, or interfere with anti-cheat systems (EAC, BattlEye, Vanguard, Tencent Anti-Cheat, etc.)
- Violate game Terms of Service, End User License Agreements, or platform policies
- Access, read, or modify memory on systems/devices you do not own or have explicit written permission to test
- Engage in unauthorized reverse engineering, tampering, or exploitation

**is strictly prohibited and may result in:**

- Permanent account and hardware bans
- Civil lawsuits from game publishers (damages, legal fees, injunctions)
- Criminal prosecution under applicable laws (CFAA in US, Computer Misuse Act in UK, EU Directive 2013/40, similar statutes worldwide)
- ISP or network termination
- Permanent damage to professional reputation

**Permitted Use Only:**
- On virtual machines, emulators, or physical devices you personally own
- In controlled lab/research environments with proper documentation
- For studying kernel internals, page table mechanics, anti-forensic techniques, or memory forensics
- With explicit written authorization for penetration testing or red-team exercises

By using, compiling, modifying, or distributing this code, you agree that:
1. You assume **full legal and ethical responsibility** for your actions.
2. The authors and contributors are **not liable** for any consequences, damages, bans, or legal issues arising from use or misuse.
3. You have read and understood all warnings in this document and the LICENSE file.

### Technical Context (For Research Purposes)

This module demonstrates:
- Direct physical memory access via page table walking
- Multi-phase behavioral obfuscation (traffic randomization, lazy registration, thermal driver mimicry)
- Per-device authentication binding
- Rate limiting and jitter to mimic legitimate driver patterns

These techniques are studied in academic security research, malware analysis, and kernel hardening studies. They do **not** guarantee undetectability against modern security mechanisms.

### Kernel-Level Requirements

This code requires:
- Linux 5.4+ or Android 10+ (kernel version varies by OEM)
- Supported architectures: ARM64/AArch64, x86_64
- CONFIG_NETLINK enabled in kernel configuration
- Unlocked bootloader for module loading

### Security Hardening Against These Techniques

**For System Administrators**
1. Enable Secure Boot: Prevent unsigned kernel modules
2. Use SELinux/AppArmor: Restrict Netlink usage
3. Enable dm-verity: Detect kernel modifications
4. Disable CONFIG_KALLSYMS: Prevent symbol enumeration
5. Use LSM (Linux Security Modules): Monitor syscalls

**For Device Manufacturers**
1. Implement kernel attestation (Measured Boot, TPM)
2. Use ARM TrustZone for security-critical operations
3. Lock bootloader with signed firmware
4. Enable SMACK or SELinux mandatory policies

### Code Quality Notes

**This Codebase Includes:**
- Well-commented implementation with error handling
- Proper kernel synchronization primitives (spinlocks, semaphores)
- Memory safety and resource cleanup
- Comprehensive documentation

**This Code Does NOT Provide:**
- Attestation against secure boot or ARM TrustZone
- Syscall hooking or filesystem redirection
- Legal protection for unauthorized use
- Guaranteed evasion against modern security mechanisms

### Responsible Disclosure

If you discover a vulnerability or security issue:
1. Do **not** disclose publicly until a reasonable fix period has passed.
2. Contact the repository maintainer privately with full details (affected kernel versions, reproduction steps, impact).
3. Allow at least 30 days for remediation before public disclosure.

### No Support for Unauthorized Use

The authors will **not** provide assistance, guidance, or support for:
- Bypassing game anti-cheat
- Modifying online games
- Evading detection in production environments
- Any non-research or unauthorized use case

All support queries must align with the educational/research purpose stated here.

> **FINAL USER ACKNOWLEDGMENT**  
> By cloning, downloading, compiling, or using this repository, you confirm that:  
> 1. You have read and fully understand this SECURITY.md and LICENSE  
> 2. You will use this code **only** for authorized research, education, or testing  
> 3. You accept **full legal responsibility** for your actions  
> 4. You will **not** use it to cheat, bypass anti-cheat, or violate ToS  
> 5. The authors are **not liable** for any consequences

**Last Updated**: February 2026  
**Status**: ✅ Legally & Ethically Reviewed  
**Jurisdiction**: User assumes all legal responsibility in their location