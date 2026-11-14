#pragma once

#include "ir.hpp"
#include "types.hpp"
#include <variant>

using std::logic_error;
using std::optional;
using std::variant;

namespace rx::IR {

using SourceTypeId = semantic::TypeId;

//ptr 变量的辅助信息，记录指向的具体信息。
struct Place {
    SourceTypeId type;
    Operand address;

    Place(SourceTypeId type, Operand address) : type(type), address(address) {
        if (address.type != Ptr) throw logic_error("a Place needs a pointer address");
    }
};

struct StorageSlot {
    SourceTypeId type = semantic::unit;
    optional<Place> location;

    StorageSlot() = default;
    StorageSlot(Place place) : type(place.type), location(place) {}
    explicit StorageSlot(SourceTypeId empty_type) : type(empty_type) {
        if (type != semantic::unit && type != semantic::never) throw logic_error("a stored type needs a location");
    }
    const Place& place() const { return location.value(); }
};

class Value {
public:
    // Unit:(), Direct: 直接的操作数，StoredAggregate: 存储的聚合体（数组、Struct、Vec）。
    enum class Repr { Unit, Direct, StoredAggregate };

    Value() = default;

    static Value direct(SourceTypeId type, Operand operand) {
        if (type == semantic::unit || type == semantic::never || operand.type == Void) throw logic_error("invalid direct value");
        return Value(Repr::Direct, type, operand);
    }

    Repr repr() const { return repr; }
    SourceTypeId type() const { return type; }
    Operand directOperand() const {
        require(Repr::Direct);
        return operand;
    }
    Operand storageAddress() const {
        require(Repr::StoredAggregate);
        return operand;
    }
    Operand abiOperand() const {
        if (repr == Repr::Unit) throw logic_error("Unit has no ABI operand");
        return operand;
    }

private:
    friend class Lowerer;

    Repr repr = Repr::Unit;
    SourceTypeId type = semantic::unit;
    Operand operand;

    Value(Repr repr, SourceTypeId type, Operand operand): repr_(repr), type_(type), operand_(operand) {}

    static Value fromAggregateStorage(SourceTypeId type, Operand address) {
        if (type == semantic::unit || type == semantic::never || address.type != Ptr) throw logic_error("invalid stored aggregate value");
        return Value(Repr::StoredAggregate, type, address);
    }

    void require(Repr expected) const {
        if (repr_ != expected) throw logic_error("wrong value representation");
    }
};

struct Terminated {};
class EvalResult {
public:
    EvalResult() : payload(Value{}) {}
    EvalResult(Value value) : payload(value) {}
    EvalResult(Place place) : payload(place) {}
    static EvalResult terminated() { return EvalResult(Terminated{}); }


    bool normal() const { return !std::holds_alternative<Terminated>(payload); }
    bool isPlace() const { return std::holds_alternative<Place>(payload); }
    SourceTypeId type() const {
        if (!normal()) return sem::never;
        return isPlace() ? place().type : value().type();
    }
    const Place& place() const { return std::get<Place>(payload); }
    const Value& value() const { return std::get<Value>(payload); }

private:
    variant<Terminated, Value, Place> payload;
    explicit EvalResult(Terminated end) : payload(end) {}
};

};