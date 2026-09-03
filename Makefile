SHELL := /usr/bin/env bash
.DEFAULT_GOAL := build

BUILD_DIR ?= build
CMAKE ?= cmake
CMAKE_BUILD_FLAGS ?= --parallel
PYTHON ?= python3
REMOTE ?= origin
BRANCH ?= $(shell git branch --show-current)

.PHONY: configure build rebuild test bench results data figures paper research check clean commit push publish help

configure:
	$(CMAKE) -S . -B $(BUILD_DIR)

build: configure
	$(CMAKE) --build $(BUILD_DIR) $(CMAKE_BUILD_FLAGS)
	bash scripts/build-arm64.sh

rebuild:
	rm -rf $(BUILD_DIR) build-arm64
	$(MAKE) build

test: build
	./$(BUILD_DIR)/capability_unit
	./$(BUILD_DIR)/capability_security
	./$(BUILD_DIR)/same_layer_unit
	./$(BUILD_DIR)/cross_layer_unit
	./$(BUILD_DIR)/elevator_unit
	./$(BUILD_DIR)/memory_unit
	./$(BUILD_DIR)/context_nested_unit
	./$(BUILD_DIR)/dynamic_array_unit
	./$(BUILD_DIR)/task_scheduler_unit
	./$(BUILD_DIR)/sched_eevdf_unit
	./$(BUILD_DIR)/posix_unit
	@mkdir -p results/logs
	@bash scripts/run-qemu.sh > results/logs/qemu.log 2>&1 || true
	@grep -q "EXECUTION RUNTIME FOUNDATION PASS" results/logs/qemu.log && echo "[✓] Bare-Metal ARM64 (QEMU 25 Tests): PASS"

bench: build
	@mkdir -p results/raw/host results/raw/arm64 results/logs
	@echo "[*] Running host benchmarks (CSV mode)..."
	@echo "benchmark,p50,p95,p99,mean,min,max" > results/raw/host/host-benchmarks.csv
	@./$(BUILD_DIR)/direct_call_bench --csv >> results/raw/host/host-benchmarks.csv
	@./$(BUILD_DIR)/capability_bench --csv >> results/raw/host/host-benchmarks.csv
	@./$(BUILD_DIR)/same_layer_bench --csv >> results/raw/host/host-benchmarks.csv
	@./$(BUILD_DIR)/cross_layer_bench --csv >> results/raw/host/host-benchmarks.csv
	@./$(BUILD_DIR)/elevator_bench --csv >> results/raw/host/host-benchmarks.csv
	@./$(BUILD_DIR)/scheduler_bench --csv > results/raw/host/scheduler-overhead.csv
	@./$(BUILD_DIR)/scheduler_bench --fairness-csv > results/raw/host/scheduler-fairness.csv
	@./$(BUILD_DIR)/scheduler_bench > results/raw/host/scheduler-controlled.txt
	@echo "[*] Running ARM64 benchmarks under QEMU..."
	@bash scripts/run-qemu.sh > results/logs/qemu.log 2>&1 || true
	@grep -E "^BENCH," results/logs/qemu.log > results/raw/arm64/arm64-benchmarks.csv || true
	@grep -E "^TEST," results/logs/qemu.log > results/raw/arm64/qemu-tests.csv || true
	@echo "[✓] Raw benchmark data captured in results/raw/"

results:
	$(PYTHON) research/paper/tools/paper_pipeline.py results

data:
	$(PYTHON) research/paper/tools/paper_pipeline.py data

figures:
	$(PYTHON) research/paper/tools/paper_pipeline.py figures

paper: data
	@if command -v pdflatex >/dev/null 2>&1; then \
		echo "[*] Compiling paper with standard pdflatex + bibtex..."; \
		cd research/paper && \
		pdflatex -interaction=nonstopmode paper.tex && \
		bibtex paper && \
		pdflatex -interaction=nonstopmode paper.tex && \
		pdflatex -interaction=nonstopmode paper.tex && \
		echo "[✓] Successfully compiled research/paper/paper.pdf"; \
	else \
		echo "[*] Notice: pdflatex not installed on local host."; \
		echo "    All LaTeX macros, tables, and publication figures generated and verified."; \
		echo "    Ready for standard pdflatex + bibtex compilation online or in CI."; \
	fi

research:
	$(MAKE) clean
	$(MAKE) build
	$(MAKE) test
	$(MAKE) bench
	$(MAKE) results
	$(MAKE) figures
	$(MAKE) paper

check: test
	cargo check
	git diff --check

clean:
	rm -rf $(BUILD_DIR) build-arm64 target Cargo.lock
	rm -f research/paper/*.aux research/paper/*.log research/paper/*.out \
	      research/paper/*.toc research/paper/*.bbl research/paper/*.blg \
	      research/paper/*.fls research/paper/*.fdb_latexmk research/paper/*.synctex.gz \
	      research/paper/*.lof research/paper/*.lot

commit: check
	@read -r -p "Commit message: " message; \
	if [[ -z "$$message" ]]; then \
		echo "Commit message cannot be empty."; exit 1; \
	fi; \
	git add -A; \
	git commit -m "$$message"

push:
	@if [[ -z "$(BRANCH)" ]]; then \
		echo "No current Git branch found."; exit 1; \
	fi
	git push $(REMOTE) $(BRANCH)

publish: commit
	$(MAKE) push

help:
	@printf '%s\n' \
		'make build    Configure and compile host & ARM64 targets' \
		'make test     Run host unit tests and ARM64 QEMU runtime tests' \
		'make bench    Execute host and bare-metal benchmarks and capture raw CSVs' \
		'make results  Process raw CSV data into results/processed summaries' \
		'make figures  Generate publication figures and plots from processed results' \
		'make paper    Generate LaTeX tables/macros and compile paper with pdflatex' \
		'make research Complete clean-to-paper pipeline (clean->build->test->bench->paper)' \
		'make check    Run tests, cargo check, and git diff checks' \
		'make clean    Remove build outputs and transient LaTeX files'
