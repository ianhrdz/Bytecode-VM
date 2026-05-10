#pragma once
#include <string>
#include "TokenType.h"

class Token {
public:
    TokenType type;
    std::string lexeme;
    int line;

    Token(TokenType type, const std::string& lexeme, int line);

    std::string toString() const;
};