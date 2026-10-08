#include <antlr4-runtime.h>
#include "RxLexer.h"
#include "RxParser.h"
#include "ast.hpp"
#include "builder.hpp"
#include "visitor.hpp"

#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: antlr_demo <file.rx>\n";
        return 2;
    }

    std::ifstream source(argv[1], std::ios::binary);
    if (!source) {
        std::cerr << "cannot open file\n";
        return 2;
    }

    antlr4::ANTLRInputStream input(source);
    rx::RxLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    rx::RxParser parser(&tokens);

    auto* tree = parser.crate();

    if (parser.getNumberOfSyntaxErrors() != 0) return 1;

    rx::AST::ASTbuilder builder;
    auto rt = builder.build(tree);
    rx::AST::ASTprinter printer;
    rt->accept(printer);

    return 0;
}