#include "Debug.h"
#include "Object.h"

#include <iomanip>
#include <iostream>

static int simpleInstruction(const std::string& name, int offset) {
    std::cout << name << "\n";
    return offset + 1;
}

static int byteInstruction(const std::string& name, const Chunk& chunk, int offset) {
    uint8_t slot = chunk.code()[offset + 1];
    std::cout << std::left << std::setw(16) << name
              << static_cast<int>(slot) << "\n";
    return offset + 2;
}

static int constantInstruction(const std::string& name, const Chunk& chunk, int offset) {
    uint8_t constant = chunk.code()[offset + 1];
    std::cout << std::left << std::setw(16) << name
              << static_cast<int>(constant) << " '"
              << chunk.constants()[constant] << "'\n";
    return offset + 2;
}

static int jumpInstruction(const std::string& name, int sign, const Chunk& chunk, int offset) {
    uint16_t jump = static_cast<uint16_t>(chunk.code()[offset + 1] << 8);
    jump |= chunk.code()[offset + 2];

    std::cout << std::left << std::setw(16) << name
              << offset << " -> " << offset + 3 + sign * jump << "\n";

    return offset + 3;
}

static int closureInstruction(const Chunk& chunk, int offset) {
    offset++;
    uint8_t constant = chunk.code()[offset++];

    std::cout << std::left << std::setw(16) << "OP_CLOSURE"
              << static_cast<int>(constant) << " '"
              << chunk.constants()[constant] << "'\n";

    auto function = as<ObjFunction>(chunk.constants()[constant]);

    for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = chunk.code()[offset++];
        int index = chunk.code()[offset++];

        std::cout << std::setw(4) << "|"
                  << std::setw(16) << ""
                  << (isLocal ? "local " : "upvalue ")
                  << index << "\n";
    }

    return offset;
}

void disassembleChunk(const Chunk& chunk, const std::string& name) {
    std::cout << "== " << name << " ==\n";

    for (int offset = 0; offset < static_cast<int>(chunk.code().size());) {
        offset = disassembleInstruction(chunk, offset);
    }
}

int disassembleInstruction(const Chunk& chunk, int offset) {
    std::cout << std::setfill('0') << std::setw(4) << offset << " ";
    std::cout << std::setfill(' ');

    if (offset > 0 && chunk.lines()[offset] == chunk.lines()[offset - 1]) {
        std::cout << "   | ";
    } else {
        std::cout << std::setw(4) << chunk.lines()[offset] << " ";
    }

    auto instruction = static_cast<OpCode>(chunk.code()[offset]);

    switch (instruction) {
        case OpCode::Constant:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OpCode::Nil:
            return simpleInstruction("OP_NIL", offset);
        case OpCode::True:
            return simpleInstruction("OP_TRUE", offset);
        case OpCode::False:
            return simpleInstruction("OP_FALSE", offset);
        case OpCode::Pop:
            return simpleInstruction("OP_POP", offset);
        case OpCode::GetLocal:
            return byteInstruction("OP_GET_LOCAL", chunk, offset);
        case OpCode::SetLocal:
            return byteInstruction("OP_SET_LOCAL", chunk, offset);
        case OpCode::GetGlobal:
            return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OpCode::DefineGlobal:
            return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OpCode::SetGlobal:
            return constantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OpCode::GetUpvalue:
            return byteInstruction("OP_GET_UPVALUE", chunk, offset);
        case OpCode::SetUpvalue:
            return byteInstruction("OP_SET_UPVALUE", chunk, offset);
        case OpCode::Equal:
            return simpleInstruction("OP_EQUAL", offset);
        case OpCode::Greater:
            return simpleInstruction("OP_GREATER", offset);
        case OpCode::Less:
            return simpleInstruction("OP_LESS", offset);
        case OpCode::Add:
            return simpleInstruction("OP_ADD", offset);
        case OpCode::Subtract:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OpCode::Multiply:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OpCode::Divide:
            return simpleInstruction("OP_DIVIDE", offset);
        case OpCode::Not:
            return simpleInstruction("OP_NOT", offset);
        case OpCode::Negate:
            return simpleInstruction("OP_NEGATE", offset);
        case OpCode::Print:
            return simpleInstruction("OP_PRINT", offset);
        case OpCode::Jump:
            return jumpInstruction("OP_JUMP", 1, chunk, offset);
        case OpCode::JumpIfFalse:
            return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OpCode::Loop:
            return jumpInstruction("OP_LOOP", -1, chunk, offset);
        case OpCode::Call:
            return byteInstruction("OP_CALL", chunk, offset);
        case OpCode::Closure:
            return closureInstruction(chunk, offset);
        case OpCode::CloseUpvalue:
            return simpleInstruction("OP_CLOSE_UPVALUE", offset);
        case OpCode::Return:
            return simpleInstruction("OP_RETURN", offset);
    }

    std::cout << "Unknown opcode " << static_cast<int>(chunk.code()[offset]) << "\n";
    return offset + 1;
}