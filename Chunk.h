#pragma once

#include <cstdint>
#include <vector>
#include "Value.h"

enum class OpCode : uint8_t {
    Constant,
    Nil,
    True,
    False,
    Pop,
    GetLocal,
    SetLocal,
    GetGlobal,
    DefineGlobal,
    SetGlobal,
    GetUpvalue,
    SetUpvalue,
    Equal,
    Greater,
    Less,
    Add,
    Subtract,
    Multiply,
    Divide,
    Not,
    Negate,
    Print,
    Jump,
    JumpIfFalse,
    Loop,
    Call,
    Closure,
    CloseUpvalue,
    Return,
};

class Chunk {
public:
    void write(uint8_t byte, int line);
    void write(OpCode op, int line);
    int addConstant(const Value& value);

    const std::vector<uint8_t>& code() const { return code_; }
    std::vector<uint8_t>& code() { return code_; }
    const std::vector<int>& lines() const { return lines_; }
    const std::vector<Value>& constants() const { return constants_; }

private:
    std::vector<uint8_t> code_;
    std::vector<int> lines_;
    std::vector<Value> constants_;
};