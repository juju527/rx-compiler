# Include after the project Makefile so the probe uses the same compiler/runtime.
AST_TEST_PROBE := target/ast-tests/ast-probe
AST_TEST_SOURCES := tests/custom/ast/probe.cpp $(filter-out src/main.cpp,$(CPP_SOURCES))

$(AST_TEST_PROBE): $(AST_TEST_SOURCES) $(CPP_HEADERS) $(ANTLR_STAMP) Makefile tests/custom/ast/Makefile
	@mkdir -p target/ast-tests
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(CPP_INCLUDE_FLAGS) \
	    $(AST_TEST_SOURCES) $(GENERATED_DIR)/*.cpp \
	    $(LDFLAGS) $(CPP_LINK_FLAGS) $(LDLIBS) -o $@
