.DEFAULT_GOAL := build

include config.mk

# Use the system C++ toolchain (GCC on the OJ, Clang on macOS).
# Command-line assignments override these defaults.
CXX ?= c++
CXXFLAGS ?= -std=c++17 -g -O0
ANTLR ?= antlr
# Optional override for an existing ANTLR 4.13.2 installation.
# By default, compile the pinned runtime sources and link them statically.
ANTLR4_RUNTIME_PREFIX ?=

CPP_BUILD_DIR := .antlr
GENERATED_DIR := generated
ANTLR_OUTPUT_DIR := $(CPP_BUILD_DIR)/generated
GRAMMAR_COPY_DIR := $(CPP_BUILD_DIR)/grammar
GENERATED_NAMES := RxLexer RxParser RxParserBaseVisitor RxParserVisitor
GENERATED_SOURCES := $(addprefix $(GENERATED_DIR)/,$(addsuffix .cpp,$(GENERATED_NAMES)))
GENERATED_HEADERS := $(addprefix $(GENERATED_DIR)/,$(addsuffix .h,$(GENERATED_NAMES)))
COMPILER := $(CPP_BUILD_DIR)/rx-compiler
CPP_SOURCES := src/main.cpp $(wildcard src/ast/*.cpp src/semantic/*.cpp)
CPP_HEADERS := $(wildcard include/ast/*.hpp include/semantic/*.hpp)
CPP_INCLUDE_FLAGS := -Iinclude -Iinclude/ast -Iinclude/semantic -I$(GENERATED_DIR)

ifneq ($(strip $(ANTLR4_RUNTIME_PREFIX)),)
CPP_INCLUDE_FLAGS += -I$(ANTLR4_RUNTIME_PREFIX)/include/antlr4-runtime
CPP_LINK_FLAGS := -L$(ANTLR4_RUNTIME_PREFIX)/lib -lantlr4-runtime \
    -Wl,-rpath,$(ANTLR4_RUNTIME_PREFIX)/lib
else
ANTLR_RUNTIME_DIR := vendor/antlr4-runtime/src
ANTLR_RUNTIME_SOURCES := $(wildcard $(ANTLR_RUNTIME_DIR)/*.cpp \
    $(ANTLR_RUNTIME_DIR)/*/*.cpp $(ANTLR_RUNTIME_DIR)/*/*/*.cpp)
ANTLR_RUNTIME_OBJECTS := $(patsubst $(ANTLR_RUNTIME_DIR)/%.cpp,$(CPP_BUILD_DIR)/runtime/%.o,$(ANTLR_RUNTIME_SOURCES))
ANTLR_RUNTIME_LIB := $(CPP_BUILD_DIR)/libantlr4-runtime.a
CPP_INCLUDE_FLAGS += -I$(ANTLR_RUNTIME_DIR) -DANTLR4CPP_STATIC -pthread
CPP_LINK_FLAGS := $(ANTLR_RUNTIME_LIB) -pthread
endif

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

# Normal builds use committed C++ sources: no generator or network is needed.
# Regenerate locally with ANTLR 4.13.2 after editing either grammar, then commit.
# Rx-prefixed copies avoid collisions with the ANTLR runtime class names.
generate: grammar/Lexer.g4 grammar/Parser.g4
	@$(ANTLR) 2>&1 | head -n 1 | grep -Fq 'Version 4.13.2' || { \
	    echo 'make generate requires ANTLR 4.13.2; set ANTLR to its command.' >&2; exit 1; }
	@mkdir -p $(GRAMMAR_COPY_DIR) $(ANTLR_OUTPUT_DIR) $(GENERATED_DIR)
	sed 's/^lexer grammar Lexer;/lexer grammar RxLexer;/' \
	    grammar/Lexer.g4 > $(GRAMMAR_COPY_DIR)/RxLexer.g4
	sed -e 's/^parser grammar Parser;/parser grammar RxParser;/' \
	    -e 's/tokenVocab=Lexer/tokenVocab=RxLexer/' \
	    grammar/Parser.g4 > $(GRAMMAR_COPY_DIR)/RxParser.g4
	$(ANTLR) -Dlanguage=Cpp -visitor -no-listener -package rx \
	    -Xexact-output-dir -o $(ANTLR_OUTPUT_DIR) $(GRAMMAR_COPY_DIR)/RxLexer.g4
	$(ANTLR) -Dlanguage=Cpp -visitor -no-listener -package rx \
	    -Xexact-output-dir -lib $(ANTLR_OUTPUT_DIR) -o $(ANTLR_OUTPUT_DIR) \
	    $(GRAMMAR_COPY_DIR)/RxParser.g4
	cp $(ANTLR_OUTPUT_DIR)/*.cpp $(ANTLR_OUTPUT_DIR)/*.h $(GENERATED_DIR)/

$(COMPILER): $(CPP_SOURCES) $(CPP_HEADERS) $(GENERATED_SOURCES) $(GENERATED_HEADERS) $(ANTLR_RUNTIME_LIB) Makefile
	@mkdir -p $(CPP_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(CPP_INCLUDE_FLAGS) \
	    $(CPP_SOURCES) $(GENERATED_SOURCES) \
	    $(LDFLAGS) $(CPP_LINK_FLAGS) $(LDLIBS) -o $@

ifeq ($(strip $(ANTLR4_RUNTIME_PREFIX)),)
$(ANTLR_RUNTIME_LIB): $(ANTLR_RUNTIME_OBJECTS)
	$(AR) rcs $@ $^

$(CPP_BUILD_DIR)/runtime/%.o: $(ANTLR_RUNTIME_DIR)/%.cpp Makefile
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -I$(ANTLR_RUNTIME_DIR) -DANTLR4CPP_STATIC -pthread \
	    -MMD -MP -c $< -o $@

-include $(ANTLR_RUNTIME_OBJECTS:.o=.d)
endif

run: build
	@test -n "$(SOURCE)" || { echo 'Usage: make run SOURCE=path/to/file.rx' >&2; exit 2; }
	$(COMPILER) "$(SOURCE)"

clean:
	rm -rf $(ANTLR_OUTPUT_DIR) $(GRAMMAR_COPY_DIR) $(CPP_BUILD_DIR)/runtime $(COMPILER).dSYM
	rm -f $(COMPILER) $(CPP_BUILD_DIR)/libantlr4-runtime.a $(CPP_BUILD_DIR)/generated.stamp

test:
	@$(PYTHON) scripts/test.py
