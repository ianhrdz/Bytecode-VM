#include "Token.h"

Token::Token(TokenType type, const std::string& lexeme, int line)
    : type(type), lexeme(lexeme), line(line) {}

std::string Token::toString() const {
    return std::to_string((int) type) + " " + lexeme;
}