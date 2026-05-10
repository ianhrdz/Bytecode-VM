// main.cpp
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "VM.h"

static void repl() {
    VM vm;
    std::string line;
    for (;;) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }
        vm.interpret(line);
    }
}

static std::string readFile(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error(std::string("Could not open file: ") + path);
    }
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

static int runFile(const char* path) {
    VM vm;
    std::string source = readFile(path);
    InterpretResult result = vm.interpret(source);
    if (result == InterpretResult::CompileError) return 65;
    if (result == InterpretResult::RuntimeError) return 70;
    return 0;
}

int main(int argc, const char* argv[]) {
    try {
        if (argc == 1) {
            repl();
            return 0;
        }
        if (argc == 2) {
            return runFile(argv[1]);
        }
        std::cerr << "Usage: cpplox [path]\n";
        return 64;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 74;
    }
}