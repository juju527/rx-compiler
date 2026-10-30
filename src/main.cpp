#include <antlr4-runtime.h>
#include "RxLexer.h"
#include "RxParser.h"
#include "ast.hpp"
#include "builder.hpp"
#include "visitor.hpp"
#include "checker.hpp"

#include <fstream>
#include <iostream>

using std::string;

int STAGE = 0;

int main(int argc, char* argv[]) {
    string filename = "";
    for (int i = 1; i < argc; i++) {
        const string arg = argv[i];
        if (arg == "--stage" && i + 1 < argc){
            if (string(argv[i + 1]) == "semantic") STAGE = 1;
            else if (string(argv[i + 1]) == "ir") STAGE = 2;
            else if (string(argv[i + 1]) == "codegen") STAGE = 3;
            else STAGE = 4;
            ++i;
        }
        else if (filename.empty() && !arg.empty() && arg.front() != '-') filename = arg;
    }

    if (filename.empty()){ std::cerr << "no input file \n"; return 2; }

    std::ifstream source(filename, std::ios::binary);
    if (!source) { std::cerr << "cannot open file\n"; return 2; }

    antlr4::ANTLRInputStream input(source);
    rx::RxLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    rx::RxParser parser(&tokens);
    auto tree = parser.crate();
    if (lexer.getNumberOfSyntaxErrors() != 0 || parser.getNumberOfSyntaxErrors() != 0) return 1;

    rx::AST::ASTbuilder builder;
    auto rt = builder.build(tree);

    try {
        rx::semantic::Checker checker;
        auto result = checker.run(*rt);
    }
    catch (const rx::semantic::Diagnostic& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "internal error: " << e.what() << std::endl;
        return 2;
    }

//    rx::AST::ASTprinter printer;
//    rt->accept(printer);

    return 0;
}
