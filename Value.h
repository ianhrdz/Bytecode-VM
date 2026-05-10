#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <ostream>

struct Obj;
struct ObjString;
struct ObjFunction;
struct ObjClosure;
struct ObjNative;
struct ObjUpvalue;

using ObjPtr = std::shared_ptr<Obj>;

struct Nil {};

class Value {
public:
    using Storage = std::variant<Nil, bool, double, ObjPtr>;

    Value();
    Value(std::nullptr_t);
    Value(bool b);
    Value(double n);
    Value(ObjPtr obj);

    bool isNil() const;
    bool isBool() const;
    bool isNumber() const;
    bool isObj() const;

    bool asBool() const;
    double asNumber() const;
    ObjPtr asObj() const;

    std::string toString() const;
    bool isFalsey() const;

    const Storage& storage() const { return value_; }

private:
    Storage value_;
};

bool valuesEqual(const Value& a, const Value& b);
std::ostream& operator<<(std::ostream& os, const Value& value);