#pragma once
#include <string>
#include "TokenType.h"
#include "LiteralValue.h"

class Token {
public:
    TokenType type;
    std::string lexeme;
    LiteralValue literal; 
    int line;

    Token(TokenType type, const std::string& lexeme,
          const LiteralValue& literal, int line);

    std::string toString() const;
};