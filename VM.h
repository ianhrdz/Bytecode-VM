// VM.h
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Object.h"
#include "Value.h"

enum class InterpretResult {
    Ok,
    CompileError,
    RuntimeError,
};

class VM {
public:
    VM();
    InterpretResult interpret(const std::string& source);

private:
    struct CallFrame {
        std::shared_ptr<ObjClosure> closure;
        size_t ip = 0;
        size_t slots = 0;
    };

    std::vector<Value> stack_;
    std::vector<CallFrame> frames_;
    std::unordered_map<std::string, Value> globals_;
    std::vector<std::shared_ptr<ObjUpvalue>> openUpvalues_;

    void resetStack();
    void push(const Value& value);
    Value pop();
    Value peek(int distance) const;

    bool call(std::shared_ptr<ObjClosure> closure, int argCount);
    bool callValue(const Value& callee, int argCount);
    InterpretResult run();
    void runtimeError(const std::string& message);

    std::shared_ptr<ObjUpvalue> captureUpvalue(size_t local);
    void closeUpvalues(size_t last);

    void defineNative(const std::string& name, NativeFn function);
};