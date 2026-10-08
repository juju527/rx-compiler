.DEFAULT_GOAL := build

include config.mk

# Local C++ compiler build. Command-line assignments override these defaults.
CXX = clang++
CXXFLAGS ?= -std=c++17 -g -O0
ANTLR ?= antlr
ANTLR4_RUNTIME_PREFIX ?= /opt/homebrew/opt/antlr4-cpp-runtime

CPP_BUILD_DIR := .antlr
GENERATED_DIR := $(CPP_BUILD_DIR)/generated
GRAMMAR_COPY_DIR := $(CPP_BUILD_DIR)/grammar
ANTLR_STAMP := $(CPP_BUILD_DIR)/generated.stamp
COMPILER := $(CPP_BUILD_DIR)/rx-compiler
CPP_SOURCES := src/main.cpp $(wildcard src/ast/*.cpp)
CPP_HEADERS := $(wildcard include/ast/*.hpp)
CPP_INCLUDE_FLAGS := -Iinclude -Iinclude/ast -I$(GENERATED_DIR) \
    -I$(ANTLR4_RUNTIME_PREFIX)/include/antlr4-runtime
CPP_LINK_FLAGS := -L$(ANTLR4_RUNTIME_PREFIX)/lib -lantlr4-runtime \
    -Wl,-rpath,$(ANTLR4_RUNTIME_PREFIX)/lib

PYTHON ?= python3
VERBOSE ?= false
# Directory paths below tests, joined with ':'; separate selections with ','.
FILTER ?=
# Optional single stage: semantic, ir, codegen, or optimization.
STAGE ?=
CLANG ?= clang-22
COMPILE_TIMEOUT ?= 30
RUN_TIMEOUT ?= 10

# Export commands as data, so shell quoting survives Make's recipe expansion.
export RX_TEST_BUILD = $(BUILD)
export RX_TEST_SEMANTIC = $(SEMANTIC)
export RX_TEST_IR = $(IR)
export RX_TEST_CODEGEN = $(CODEGEN)
export RX_TEST_RUN = $(RUN)
export FILTER STAGE CLANG COMPILE_TIMEOUT RUN_TIMEOUT VERBOSE

.PHONY: build generate run clean test

build: $(COMPILER)

generate: $(ANTLR_STAMP)

# Rx-prefixed copies avoid collisions with the ANTLR runtime class names.
$(ANTLR_STAMP): grammar/Lexer.g4 grammar/Parser.g4 Makefile
	@mkdir -p $(GRAMMAR_COPY_DIR) $(GENERATED_DIR)
	sed 's/^lexer grammar Lexer;/lexer grammar RxLexer;/' \
	    grammar/Lexer.g4 > $(GRAMMAR_COPY_DIR)/RxLexer.g4
	sed -e 's/^parser grammar Parser;/parser grammar RxParser;/' \
	    -e 's/tokenVocab=Lexer/tokenVocab=RxLexer/' \
	    grammar/Parser.g4 > $(GRAMMAR_COPY_DIR)/RxParser.g4
	$(ANTLR) -Dlanguage=Cpp -visitor -no-listener -package rx \
	    -Xexact-output-dir -o $(GENERATED_DIR) $(GRAMMAR_COPY_DIR)/RxLexer.g4
	$(ANTLR) -Dlanguage=Cpp -visitor -no-listener -package rx \
	    -Xexact-output-dir -lib $(GENERATED_DIR) -o $(GENERATED_DIR) \
	    $(GRAMMAR_COPY_DIR)/RxParser.g4
	@touch $@

$(COMPILER): $(CPP_SOURCES) $(CPP_HEADERS) $(ANTLR_STAMP) Makefile
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(CPP_INCLUDE_FLAGS) \
	    $(CPP_SOURCES) $(GENERATED_DIR)/*.cpp \
	    $(LDFLAGS) $(CPP_LINK_FLAGS) $(LDLIBS) -o $@

run: build
	@test -n "$(SOURCE)" || { echo 'Usage: make run SOURCE=path/to/file.rx' >&2; exit 2; }
	$(COMPILER) "$(SOURCE)"

clean:
	rm -rf $(GENERATED_DIR) $(GRAMMAR_COPY_DIR) $(COMPILER).dSYM
	rm -f $(COMPILER) $(ANTLR_STAMP)

test:
	@$(PYTHON) scripts/test.py
