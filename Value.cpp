// Value.cpp
#include "Value.h"
#include "Object.h"

#include <cmath>
#include <sstream>

Value::Value() : value_(Nil{}) {}
Value::Value(std::nullptr_t) : value_(Nil{}) {}
Value::Value(bool b) : value_(b) {}
Value::Value(double n) : value_(n) {}
Value::Value(ObjPtr obj) : value_(std::move(obj)) {}

bool Value::isNil() const {
    return std::holds_alternative<Nil>(value_);
}

bool Value::isBool() const {
    return std::holds_alternative<bool>(value_);
}

bool Value::isNumber() const {
    return std::holds_alternative<double>(value_);
}

bool Value::isObj() const {
    return std::holds_alternative<ObjPtr>(value_);
}

bool Value::asBool() const {
    return std::get<bool>(value_);
}

double Value::asNumber() const {
    return std::get<double>(value_);
}

ObjPtr Value::asObj() const {
    return std::get<ObjPtr>(value_);
}

bool Value::isFalsey() const {
    return isNil() || (isBool() && !asBool());
}

std::string Value::toString() const {
    if (isNil()) return "nil";
    if (isBool()) return asBool() ? "true" : "false";
    if (isNumber()) {
        std::ostringstream out;
        out << asNumber();
        return out.str();
    }
    if (isObj()) {
        ObjPtr object = asObj();
        return object ? object->toString() : "nil";
    }
    return "<unknown>";
}

bool valuesEqual(const Value& a, const Value& b) {
    if (a.storage().index() != b.storage().index()) return false;

    if (a.isNil()) return true;
    if (a.isBool()) return a.asBool() == b.asBool();
    if (a.isNumber()) return a.asNumber() == b.asNumber();

    if (a.isObj()) {
        ObjPtr ao = a.asObj();
        ObjPtr bo = b.asObj();

        if (ao == bo) return true;
        if (!ao || !bo) return false;

        if (ao->type == ObjType::String && bo->type == ObjType::String) {
            auto as = std::dynamic_pointer_cast<ObjString>(ao);
            auto bs = std::dynamic_pointer_cast<ObjString>(bo);
            return as->chars == bs->chars;
        }

        return false;
    }

    return false;
}

std::ostream& operator<<(std::ostream& os, const Value& value) {
    os << value.toString();
    return os;
}