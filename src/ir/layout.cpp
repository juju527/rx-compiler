#include "lower.hpp"
#include <algorithm>

using std::max;

namespace rx::IR {

semantic::Type Lowerer::sourceType(SourceTypeId type) const {return semantic.types[type];}

bool Lowerer::isAggregate(SourceTypeId type) const {
    auto kind = sourceType(type).kind;
    return kind == semantic::TypeKind::Array || kind == semantic::TypeKind::Struct || kind == semantic::TypeKind::Vec;
}

StorageLayout Lowerer::storageLayout(SourceTypeId id) {
    auto found = layouts.find(id);
    if (found != layouts.end()) return found->second;
    const auto type = sourceType(id);
    StorageLayout result{Void, 0, 1};
    auto checked_size = [&](uint64_t size) {
        if (size > UINT32_MAX) fail("layout exceeds RV32 address space");
        return uint32_t(size);
    };
    auto aligned_size = [&](uint64_t size, uint32_t alignment) {
        return checked_size((size + alignment - 1) / alignment * alignment);
    };

    switch (type.kind) {
    case sem::TypeKind::Bool: result = {I8, 1, 1}; break;
    case sem::TypeKind::I32:
    case sem::TypeKind::U32:
    case sem::TypeKind::ISize:
    case sem::TypeKind::USize: result = {I32, 4, 4}; break;
    case sem::TypeKind::Reference:
    case sem::TypeKind::Box: result = {Ptr, 4, 4}; break;
    case sem::TypeKind::Unit: result = {module.array(I8, 0), 0, 1}; break;
    case sem::TypeKind::Vec: {
        // Vec 分成 data, length, capacity 三个字段。在 IR 中 Vec<T> 都可以视作同一类型。
        TypeId storage = Void;
        for (size_t i = 0; i < module.types.size(); i++) if (module.types[i].name == "rx.vec") storage = i;
        if (storage == Void) storage = module.structure("rx.vec", {Ptr, I32, I32});
        result = {storage, 12, 4};
        break;
    }
    case sem::TypeKind::Array: {
        auto element = storageLayout(type.element);
        auto size = checked_size(uint64_t(element.size) * type.length);
        result = {module.array(element.storage_type, type.length), size, element.alignment};
        break;
    }
    case sem::TypeKind::Struct: {
        vector<TypeId> fields;
        for (const auto& field : semantic.structs.at(type.declaration).fields) {
            auto member = storageLayout(field.type);
            result.size = aligned_size(result.size, member.alignment);
            result.size = checked_size(uint64_t(result.size) + member.size);
            result.alignment = max(result.alignment, member.alignment);
            fields.push_back(member.storage_type);
        }
        result.size = aligned_size(result.size, result.alignment);
        result.storage_type = module.structure("rx.struct." + to_string(type.declaration), move(fields));
        break;
    }
    default: fail("unknown/never has no runtime layout");
    }
    layouts.emplace(id, result);
    return result;
}

TypeId Lowerer::valueReprType(SourceTypeId type) {
    if (type == semantic::unit || type == semantic::never) return Void;
    if (type == semantic::boolean) return I1;
    return isAggregate(type) ? Ptr : storageLayout(type).storage_type;
}


}