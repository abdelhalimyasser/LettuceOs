# Security Policy

## Research Prototype Status

**Lettuce is an academic research operating system prototype.** It is designed to explore architectural isolation mechanisms, capability dispatch, and hardware security features under laboratory conditions.

It has **not** been formally verified, security-audited, or certified for production deployment or critical infrastructure environments.

---

## Reporting a Vulnerability

If you discover a security flaw, architectural isolation bypass, or memory vulnerability in the Lettuce prototype:

1. Please do **not** file public issues on GitHub.
2. Email details and reproduction steps to the primary maintainer:
   - **Contact:** Abdelhalim Yasser Abdelhalim
   - **Email:** `212400555@fci.capu.edu.eg`
3. Include:
   - Specific commit hash / branch.
   - Affected subsystem (MMU, PAC, Capabilities, Scheduler, Syscalls).
   - Minimal reproduction script or test case.
   - Observed behavior vs. expected isolation fault.

We will acknowledge receipt and collaborate on evaluating the theoretical or architectural impact on the prototype.
