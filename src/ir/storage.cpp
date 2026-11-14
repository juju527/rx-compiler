#include "lower.hpp"

namespace rx::IR {

StorageSlot Lowerer::createStorageSlot(SourceTypeId type) {
    if (type == semantic::unit || type == semantic::never) return StorageSlot{type};
    return Place{type, builder().slot(storageLayout(type).storage_type)};
}

Value Lowerer::loadScalar(Place place) {
    if (place.type == semantic::unit) return Value{};
    if (isAggregate(place.type)) fail("loadScalar cannot load an aggregate");
    auto operand = builder().load(storageLayout(place.type).storage_type, place.address);
    if (place.type == semantic::boolean) operand = builder().convert(Opcode::Trunc, operand, I1);// i8 存储，转成 i1。
    return Value::direct(place.type, operand);
}

void Lowerer::copyRepresentation(SourceTypeId type, Operand destination, Operand source) {
    auto bytes = storageLayout(type).size;
    if (!bytes) return ;
    builder().call("memmove", {destination, source, constant(I32, bytes)});
    return ;
}

Value Lowerer::snapshot(const EvalResult& result) {
    if (!result.normal()) fail("cannot obtain a value from a terminated expression");
    if (!result.isPlace()) return result.value();

    const auto place = result.place();
    if (!isAggregate(place.type)) return loadScalar(place);
    auto saved = createStorageSlot(place.type).place();
    copyRepresentation(place.type, saved.address, place.address);
    return Value::fromAggregateStorage(place.type, saved.address);
}

Value Lowerer::valueFromResultSlot(const StorageSlot& slot) {
    if (slot.type == semantic::never) fail("nonreturning expression has no result storage");
    if (slot.type == semantic::unit) return Value{};
    auto place = slot.place();
    if (!isAggregate(place.type)) return loadScalar(place);
    return Value::fromAggregateStorage(place.type, place.address);
}

void Lowerer::storeValue(const StorageSlot& destination, Value value) {
    if (value.repr() == Value::Repr::Unit) return ;
    auto place = destination.place();
    if (value.repr() == Value::Repr::StoredAggregate) {
        copyRepresentation(value.type(), place.address, value.storageAddress());
        return;
    }
    auto operand = value.directOperand();
    if (value.type() == semantic::boolean) operand = builder().convert(Opcode::ZExt, operand, I8);
    builder().store(operand, place.address);
    return ;
}

Place Lowerer::materialize(const EvalResult& result) {
    if (!result.normal()) fail("cannot materialize a terminated expression");
    if (result.isPlace()) return result.place();
    const auto& value = result.value();
    if (value.repr() == Value::Repr::StoredAggregate) return {value.type(), value.storageAddress()};
    Place slot{value.type(), builder().slot(storageLayout(value.type()).storage_type)};
    storeValue(slot, value);
    return slot;
}

void Lowerer::copyPlaceContents(Place destination, Place source) {
    if (destination.type != source.type) fail("copy source type mismatch");
    if (isAggregate(source.type)) copyRepresentation(source.type, destination.address, source.address);
    else storeValue(destination, loadScalar(source));
    return ;
}

}