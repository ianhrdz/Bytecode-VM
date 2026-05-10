// VM.cpp
#include "VM.h"

#include "Common.h"
#include "Compiler.h"
#include "Debug.h"

#include <chrono>
#include <iostream>
#include <stdexcept>

static std::string asStringName(const Value& value) {
    auto string = as<ObjString>(value);
    if (!string) return "";
    return string->chars;
}

VM::VM() {
    defineNative("clock", [](int, const Value*) -> Value {
        using clock = std::chrono::system_clock;
        auto now = clock::now().time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        return static_cast<double>(millis) / 1000.0;
    });
}

InterpretResult VM::interpret(const std::string& source) {
    std::shared_ptr<ObjFunction> function = compile(source);

    if (!function) {
        return InterpretResult::CompileError;
    }

    auto closure = std::make_shared<ObjClosure>(function);
    push(std::static_pointer_cast<Obj>(closure));
    call(closure, 0);

    return run();
}

void VM::resetStack() {
    stack_.clear();
    frames_.clear();
    openUpvalues_.clear();
}

void VM::push(const Value& value) {
    stack_.push_back(value);
}

Value VM::pop() {
    Value value = stack_.back();
    stack_.pop_back();
    return value;
}

Value VM::peek(int distance) const {
    return stack_[stack_.size() - 1 - distance];
}

void VM::defineNative(const std::string& name, NativeFn function) {
    auto native = std::make_shared<ObjNative>(std::move(function));
    globals_[name] = std::static_pointer_cast<Obj>(native);
}

bool VM::call(std::shared_ptr<ObjClosure> closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError(
            "Expected " +
            std::to_string(closure->function->arity) +
            " arguments but got " +
            std::to_string(argCount) +
            "."
        );
        return false;
    }

    if (frames_.size() == 64) {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame frame;
    frame.closure = std::move(closure);
    frame.ip = 0;
    frame.slots = stack_.size() - argCount - 1;

    frames_.push_back(frame);
    return true;
}

bool VM::callValue(const Value& callee, int argCount) {
    if (callee.isObj()) {
        switch (callee.asObj()->type) {
            case ObjType::Closure:
                return call(as<ObjClosure>(callee), argCount);

            case ObjType::Native: {
                auto native = as<ObjNative>(callee);
                Value result = native->function(
                    argCount,
                    &stack_[stack_.size() - argCount]
                );

                stack_.resize(stack_.size() - argCount - 1);
                push(result);
                return true;
            }

            default:
                break;
        }
    }

    runtimeError("Can only call functions and classes.");
    return false;
}

std::shared_ptr<ObjUpvalue> VM::captureUpvalue(size_t local) {
    for (auto& upvalue : openUpvalues_) {
        if (!upvalue->isClosed && upvalue->location == local) {
            return upvalue;
        }
    }

    auto created = std::make_shared<ObjUpvalue>(local);
    openUpvalues_.push_back(created);
    return created;
}

void VM::closeUpvalues(size_t last) {
    for (auto& upvalue : openUpvalues_) {
        if (!upvalue->isClosed && upvalue->location >= last) {
            upvalue->closed = stack_[upvalue->location];
            upvalue->isClosed = true;
        }
    }
}

void VM::runtimeError(const std::string& message) {
    std::cerr << message << "\n";

    for (int i = static_cast<int>(frames_.size()) - 1; i >= 0; i--) {
        CallFrame& frame = frames_[i];
        auto function = frame.closure->function;

        size_t instruction = frame.ip == 0 ? 0 : frame.ip - 1;
        int line = function->chunk.lines()[instruction];

        std::cerr << "[line " << line << "] in ";

        if (function->name.empty()) {
            std::cerr << "script\n";
        } else {
            std::cerr << function->name << "()\n";
        }
    }

    resetStack();
}

InterpretResult VM::run() {
    auto readByte = [&]() -> uint8_t {
        CallFrame& frame = frames_.back();
        return frame.closure->function->chunk.code()[frame.ip++];
    };

    auto readShort = [&]() -> uint16_t {
        uint8_t high = readByte();
        uint8_t low = readByte();
        return static_cast<uint16_t>((high << 8) | low);
    };

    auto readConstant = [&]() -> Value {
        CallFrame& frame = frames_.back();
        return frame.closure->function->chunk.constants()[readByte()];
    };

    auto binaryNumberOp = [&](OpCode op) -> bool {
        if (!peek(0).isNumber() || !peek(1).isNumber()) {
            runtimeError("Operands must be numbers.");
            return false;
        }

        double b = pop().asNumber();
        double a = pop().asNumber();

        switch (op) {
            case OpCode::Greater:
                push(a > b);
                break;
            case OpCode::Less:
                push(a < b);
                break;
            case OpCode::Add:
                push(a + b);
                break;
            case OpCode::Subtract:
                push(a - b);
                break;
            case OpCode::Multiply:
                push(a * b);
                break;
            case OpCode::Divide:
                push(a / b);
                break;
            default:
                return false;
        }

        return true;
    };

    for (;;) {
        CallFrame& frame = frames_.back();

#if DEBUG_TRACE_EXECUTION
        std::cout << "          ";
        for (const Value& slot : stack_) {
            std::cout << "[ " << slot << " ]";
        }
        std::cout << "\n";
        disassembleInstruction(frame.closure->function->chunk, static_cast<int>(frame.ip));
#endif

        auto instruction = static_cast<OpCode>(readByte());

        switch (instruction) {
            case OpCode::Constant: {
                Value constant = readConstant();
                push(constant);
                break;
            }

            case OpCode::Nil:
                push(nullptr);
                break;

            case OpCode::True:
                push(true);
                break;

            case OpCode::False:
                push(false);
                break;

            case OpCode::Pop:
                pop();
                break;

            case OpCode::GetLocal: {
                uint8_t slot = readByte();
                push(stack_[frame.slots + slot]);
                break;
            }

            case OpCode::SetLocal: {
                uint8_t slot = readByte();
                stack_[frame.slots + slot] = peek(0);
                break;
            }

            case OpCode::GetGlobal: {
                Value nameValue = readConstant();
                std::string name = asStringName(nameValue);

                auto found = globals_.find(name);

                if (found == globals_.end()) {
                    runtimeError("Undefined variable '" + name + "'.");
                    return InterpretResult::RuntimeError;
                }

                push(found->second);
                break;
            }

            case OpCode::DefineGlobal: {
                Value nameValue = readConstant();
                std::string name = asStringName(nameValue);

                globals_[name] = peek(0);
                pop();
                break;
            }

            case OpCode::SetGlobal: {
                Value nameValue = readConstant();
                std::string name = asStringName(nameValue);

                auto found = globals_.find(name);

                if (found == globals_.end()) {
                    runtimeError("Undefined variable '" + name + "'.");
                    return InterpretResult::RuntimeError;
                }

                found->second = peek(0);
                break;
            }

            case OpCode::GetUpvalue: {
                uint8_t slot = readByte();
                auto upvalue = frame.closure->upvalues[slot];

                if (upvalue->isClosed) {
                    push(upvalue->closed);
                } else {
                    push(stack_[upvalue->location]);
                }

                break;
            }

            case OpCode::SetUpvalue: {
                uint8_t slot = readByte();
                auto upvalue = frame.closure->upvalues[slot];

                if (upvalue->isClosed) {
                    upvalue->closed = peek(0);
                } else {
                    stack_[upvalue->location] = peek(0);
                }

                break;
            }

            case OpCode::Equal: {
                Value b = pop();
                Value a = pop();
                push(valuesEqual(a, b));
                break;
            }

            case OpCode::Greater:
            case OpCode::Less:
            case OpCode::Subtract:
            case OpCode::Multiply:
            case OpCode::Divide:
                if (!binaryNumberOp(instruction)) {
                    return InterpretResult::RuntimeError;
                }
                break;

            case OpCode::Add: {
                if (isObjType(peek(0), ObjType::String) &&
                    isObjType(peek(1), ObjType::String)) {
                    auto b = as<ObjString>(pop());
                    auto a = as<ObjString>(pop());

                    push(std::static_pointer_cast<Obj>(
                        std::make_shared<ObjString>(a->chars + b->chars)
                    ));
                } else if (peek(0).isNumber() && peek(1).isNumber()) {
                    if (!binaryNumberOp(OpCode::Add)) {
                        return InterpretResult::RuntimeError;
                    }
                } else {
                    runtimeError("Operands must be two numbers or two strings.");
                    return InterpretResult::RuntimeError;
                }

                break;
            }

            case OpCode::Not:
                push(pop().isFalsey());
                break;

            case OpCode::Negate:
                if (!peek(0).isNumber()) {
                    runtimeError("Operand must be a number.");
                    return InterpretResult::RuntimeError;
                }

                push(-pop().asNumber());
                break;

            case OpCode::Print:
                std::cout << pop() << "\n";
                break;

            case OpCode::Jump: {
                uint16_t offset = readShort();
                frame.ip += offset;
                break;
            }

            case OpCode::JumpIfFalse: {
                uint16_t offset = readShort();

                if (peek(0).isFalsey()) {
                    frame.ip += offset;
                }

                break;
            }

            case OpCode::Loop: {
                uint16_t offset = readShort();
                frame.ip -= offset;
                break;
            }

            case OpCode::Call: {
                int argCount = readByte();

                if (!callValue(peek(argCount), argCount)) {
                    return InterpretResult::RuntimeError;
                }

                break;
            }

            case OpCode::Closure: {
                Value functionValue = readConstant();
                auto function = as<ObjFunction>(functionValue);

                auto closure = std::make_shared<ObjClosure>(function);
                closure->upvalues.resize(function->upvalueCount);

                push(std::static_pointer_cast<Obj>(closure));

                for (int i = 0; i < function->upvalueCount; i++) {
                    bool isLocal = readByte() == 1;
                    uint8_t index = readByte();

                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(frame.slots + index);
                    } else {
                        closure->upvalues[i] = frame.closure->upvalues[index];
                    }
                }

                break;
            }

            case OpCode::CloseUpvalue:
                closeUpvalues(stack_.size() - 1);
                pop();
                break;

            case OpCode::Return: {
                Value result = pop();

                closeUpvalues(frame.slots);

                size_t slots = frame.slots;
                frames_.pop_back();

                if (frames_.empty()) {
                    pop();
                    return InterpretResult::Ok;
                }

                stack_.resize(slots);
                push(result);
                break;
            }
        }
    }
}