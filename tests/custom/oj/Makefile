# Run from the repository root. Load the judge commands without the project
# Makefile so references to build-only variables cannot pass unnoticed.
.DEFAULT_GOAL := test

include config.mk

PYTHON ?= python3
export RX_TEST_BUILD = $(BUILD)
export RX_TEST_SEMANTIC = $(SEMANTIC)
export RX_TEST_IR = $(IR)
export RX_TEST_CODEGEN = $(CODEGEN)
export RX_TEST_RUN = $(RUN)
export FILTER STAGE VERBOSE

.PHONY: test
test:
	@$(PYTHON) scripts/test.py
