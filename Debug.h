#pragma once

#include <string>
#include "Chunk.h"

void disassembleChunk(const Chunk& chunk, const std::string& name);
int disassembleInstruction(const Chunk& chunk, int offset);