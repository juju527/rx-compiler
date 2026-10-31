# ANTLR C++ runtime 4.13.2

This directory vendors the unmodified files in `runtime/src/` (excluding IDE
metadata), plus `VERSION` and `LICENSE.txt`, from the official release:

- Source: https://www.antlr.org/download/antlr4-cpp-runtime-4.13.2-source.zip
- Archive SHA-256: `0ed13668906e86dbc0dcddf30fdee68c10203dea4e83852b4edb810821bee3c4`
- License: BSD-3-Clause; see [LICENSE.txt](LICENSE.txt).

The project Makefile compiles these sources into a static library. This keeps
OJ builds offline and pins the runtime to the same version as the committed
Lexer/Parser sources. No system ANTLR installation or CMake is required.

Keep the runtime and generator versions in sync when upgrading.
