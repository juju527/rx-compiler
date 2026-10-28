#include "checker.hpp"

namespace rx::semantic {
using namespace rx::AST;

void Checker::fail(NodeId node, const string& message) const { throw Diagnostic(node, message); }

SemanticResult analyze(const Crate& crate) { return Checker{}.run(crate); }

SemanticResult Checker::run(const Crate& crate) {
    collect(crate);
    declarations();
    checkLayouts();
    checkDerives();
    
    auto main = globals.find("main");
    if (main == globals.end() || main->second.kind != NameKind::Function) fail(crate.id, "main function error");
    p.entry = main->second.id;
    const auto& main_syntax = *functions.at(p.entry).node;
    const auto& main_signature = p.functions.at(p.entry).signature;
    if (!main_signature.parameters.empty() || main_signature.result != unit || main_syntax.has_lifetime)
        fail(p.entry, "main must have no parameters or lifetime parameters and return ()");
    
    for (const auto& entry : functions) functionBody(entry.first);

    for (TypeId id = unit; id < p.types.size(); ++id)
        for (auto cap : {Capability::Copy, Capability::Clone, Capability::PartialEq, Capability::Eq})
            capable(id, cap);
    return std::move(p);
}


};