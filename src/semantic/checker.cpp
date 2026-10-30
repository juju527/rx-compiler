#include "checker.hpp"

using std::move;

namespace rx::semantic {
using namespace rx::AST;

void Checker::fail(NodeId node, const string& message) const { throw Diagnostic(node, message); }

SemanticResult analyze(const Crate& crate) { return Checker{}.run(crate); }

SemanticResult Checker::run(const Crate& crate) {
    collect(crate);
    declarations();
    checkLayouts();
    checkDerives();
    
    auto main = scopes[0].find("main");
    if (main == scopes[0].end() || main->second.kind != NameKind::Function) fail(crate.id, "main function error");
    result.entry = main->second.id;
    const auto& main_syntax = *functions.at(result.entry).node;
    const auto& main_signature = result.functions.at(result.entry).signature;
    if (!main_signature.parameters.empty() || main_signature.result != unit || main_syntax.has_lifetime)
        fail(result.entry, "main must have no parameters or lifetime parameters and return ()");
    
    for (const auto& entry : functions) functionBody(entry.first);

    for (TypeId id = unit; id < result.types.size(); ++id)
        for (auto cap : {Capability::Copy, Capability::Clone, Capability::PartialEq, Capability::Eq})
            capable(id, cap);
    return move(result);
}


};