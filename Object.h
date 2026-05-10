#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "Chunk.h"
#include "Value.h"

enum class ObjType {
    String,
    Function,
    Native,
    Closure,
    Upvalue,
};

struct Obj {
    explicit Obj(ObjType type) : type(type) {}
    virtual ~Obj() = default;
    ObjType type;
    virtual std::string toString() const = 0;
};

struct ObjString : Obj {
    explicit ObjString(std::string chars) : Obj(ObjType::String), chars(std::move(chars)) {}
    std::string chars;
    std::string toString() const override { return chars; }
};

struct ObjFunction : Obj {
    ObjFunction() : Obj(ObjType::Function) {}
    int arity = 0;
    int upvalueCount = 0;
    std::string name;
    Chunk chunk;

    std::string toString() const override;
};

using NativeFn = std::function<Value(int argCount, const Value* args)>;

struct ObjNative : Obj {
    explicit ObjNative(NativeFn function) : Obj(ObjType::Native), function(std::move(function)) {}
    NativeFn function;
    std::string toString() const override { return "<native fn>"; }
};

struct ObjUpvalue : Obj {
    explicit ObjUpvalue(size_t location) : Obj(ObjType::Upvalue), location(location) {}
    size_t location;
    Value closed;
    bool isClosed = false;
    std::string toString() const override { return "upvalue"; }
};

struct ObjClosure : Obj {
    explicit ObjClosure(std::shared_ptr<ObjFunction> function)
        : Obj(ObjType::Closure), function(std::move(function)) {}
    std::shared_ptr<ObjFunction> function;
    std::vector<std::shared_ptr<ObjUpvalue>> upvalues;

    std::string toString() const override;
};

template <typename T>
std::shared_ptr<T> as(const Value& value) {
    return std::dynamic_pointer_cast<T>(value.asObj());
}

inline bool isObjType(const Value& value, ObjType type) {
    return value.isObj() && value.asObj() && value.asObj()->type == type;
}