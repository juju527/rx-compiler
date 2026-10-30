# Project compiler commands. See README-EN.md or README-ZH.md for details.
# Run the implemented stage with: make test STAGE=semantic FILTER=official:semantic

# Optional: build your compiler once before testing. Leave empty if prebuilt.
# {runtime} will be linked into every program.
# Write shared runtime assembly there if your IR/CODEGEN output references it.
BUILD = $(MAKE) build

# Required for semantic tests: exit 0 to accept {source}, 1 to reject it.
SEMANTIC = $(COMPILER) --stage semantic {source}

# Configure when IR generation is implemented: write RV32 LLVM IR to {output} (.ll).
IR =

# Configure when code generation is implemented: write RV32IM assembly to {output} (.s).
CODEGEN =

# Required alongside IR/CODEGEN: run the assembly with the BUILD runtime in REIMU.
RUN = xmake run -P vendor/REIMU reimu --memory=256M --stack=1M \
    -f {output},{runtime} -o {stdout} -p {profile} 1>&2
