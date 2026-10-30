#include "checker.hpp"

#include <functional>

namespace rx::semantic {

bool supportsDeclared(const SemanticResult& res, TypeId id, Capability capability) {
    const auto t = res.types[id];
    switch (t.kind) {
    case TypeKind::Unknown: return false;
    case TypeKind::Struct:
        return res.structs.at(t.declaration).derives[static_cast<size_t>(capability)];
    case TypeKind::Reference:
        if (capability == Capability::Copy || capability == Capability::Clone) return !t.mutable_reference;
        return supportsDeclared(res, t.element, capability);
    case TypeKind::Box: case TypeKind::Vec:
        return capability != Capability::Copy && supportsDeclared(res, t.element, capability);
    case TypeKind::Array:
        return supportsDeclared(res, t.element, capability);
    default: return true; // 整数、bool、unit 和 never。
    }
}

// run 保证全部 derive 验证成功后才调用
bool Checker::capable(TypeId type, Capability capability) {
    const auto cap = static_cast<size_t>(capability);
    auto& cache = result.capabilities[cap];
    if (auto found = cache.find(type); found != cache.end()) return found->second;
    const bool res = supportsDeclared(result, type, capability);
    cache[type] = res;
    return res;
}

void Checker::checkDerives() {
    for (const auto& entry : result.structs) {
        const auto& s = entry.second;
        if (s.derives[0] && !s.derives[1]) fail(entry.first, "Copy requires an explicit Clone derive");
        if (s.derives[3] && !s.derives[2]) fail(entry.first, "Eq requires an explicit PartialEq derive");
        const string names[4] = {"Copy", "Clone", "PartialEq", "Eq"};
        for (size_t index = 0; index < 4; ++index) {
            if (!s.derives[index]) continue;
            for (const auto& field : s.fields)
                if (!supportsDeclared(result, field.type, static_cast<Capability>(index)))
                    fail(entry.first,  "field '" + field.name + "' of '" + s.name + "' does not support " + names[index]);
        }
    }
}

};