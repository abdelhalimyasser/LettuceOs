# Repository Readiness

The public repository contains source, public engineering documentation,
canonical raw evidence under [`results/raw/`](../results/raw/), and derived
public summaries under [`results/processed/`](../results/processed/).

Host validation consists of 11 unit/security suites. The original local host
measurements were collected on an Intel Core i5-1145G7 (`x86_64`). ARM64
validation runs 25 runtime tests under QEMU TCG. GitHub-hosted runners provide
separate CI portability evidence. QEMU logs are local diagnostics, not
canonical scientific evidence; Generic Counter ticks are emulator-relative.

Use [`reproduce.md`](reproduce.md) for supported build and validation commands.
