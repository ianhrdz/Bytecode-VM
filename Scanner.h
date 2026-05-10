// Scanner.h
#pragma once

#include <string>

enum class TokenType {
    LeftParen, RightParen, LeftBrace, RightBrace,
    Comma, Dot, Minus, Plus, Semicolon, Slash, Star,
    Bang, BangEqual,
    Equal, EqualEqual,
    Greater, GreaterEqual,
    Less, LessEqual,
    Identifier, String, Number,
    And, Class, Else, False, For, Fun, If, Nil, Or,
    Print, Return, Super, This, True, Var, While,
    Error, Eof,
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line = 1;
};

class Scanner {
public:
    explicit Scanner(std::string source);
    Token scanToken();

private:
    std::string source_;
    size_t start_ = 0;
    size_t current_ = 0;
    int line_ = 1;

    bool isAtEnd() const;
    char advance();
    bool match(char expected);
    char peek() const;
    char peekNext() const;
    void skipWhitespace();

    Token makeToken(TokenType type) const;
    Token errorToken(const std::string& message) const;
    Token string();
    Token number();
    Token identifier();
    TokenType identifierType() const;
};