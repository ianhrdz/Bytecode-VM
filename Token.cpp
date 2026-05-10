#include "Token.h"

Token::Token(TokenType type, const std::string& lexeme,
             const LiteralValue& literal, int line)
    : type(type), lexeme(lexeme), literal(literal), line(line) {}

std::string Token::toString() const {
    return std::to_string((int)type) + " " + lexeme;
}