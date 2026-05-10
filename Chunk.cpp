#include "Chunk.h"

void Chunk::write(uint8_t byte, int line) {
    code_.push_back(byte);
    lines_.push_back(line);
}

void Chunk::write(OpCode op, int line) {
    write(static_cast<uint8_t>(op), line);
}

int Chunk::addConstant(const Value& value) {
    constants_.push_back(value);
    return static_cast<int>(constants_.size() - 1);
}