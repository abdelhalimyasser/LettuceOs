# AI Agent Guidelines & Project Skills

This directory contains specialized workflows, checklists, and skills for autonomous coding agents contributing to the Lettuce operating system codebase.

---

## Directory Layout

```text
.agents/
├── README.md
└── skills/
    ├── lettuce-architecture/    # System classification, layers, and communication topologies
    │   └── SKILL.md
    ├── arm64-low-level/         # Privilege boundaries, MMU, ASIDs, vectors, and assembly
    │   └── SKILL.md
    ├── kernel-security/         # Capability enforcement, PAC, invariants, and threat models
    │   └── SKILL.md
    ├── testing-and-qemu/        # Host testing, ARM64 cross-compilation, and QEMU workflows
    │   └── SKILL.md
    └── research-and-docs/       # Public documentation integrity and benchmark reporting
        └── SKILL.md
```

All agents should read [`AGENTS.md`](../AGENTS.md) as the root engineering specification before applying these skills.
