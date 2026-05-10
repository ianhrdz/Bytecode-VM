#include "Scanner.h"

#include <cctype>
#include <unordered_map>

Scanner::Scanner(std::string source)
    : source_(std::move(source)) {}

bool Scanner::isAtEnd() const {
    return current_ >= source_.size();
}

char Scanner::advance() {
    return source_[current_++];
}

bool Scanner::match(char expected) {
    if (isAtEnd()) return false;
    if (source_[current_] != expected) return false;
    current_++;
    return true;
}

char Scanner::peek() const {
    if (isAtEnd()) return '\0';
    return source_[current_];
}

char Scanner::peekNext() const {
    if (current_ + 1 >= source_.size()) return '\0';
    return source_[current_ + 1];
}

Token Scanner::makeToken(TokenType type) const {
    return Token{
        type,
        source_.substr(start_, current_ - start_),
        line_,
    };
}

Token Scanner::errorToken(const std::string& message) const {
    return Token{
        TokenType::Error,
        message,
        line_,
    };
}

void Scanner::skipWhitespace() {
    for (;;) {
        char c = peek();

        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;

            case '\n':
                line_++;
                advance();
                break;

            case '/':
                if (peekNext() == '/') {
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else {
                    return;
                }
                break;

            default:
                return;
        }
    }
}

Token Scanner::string() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') line_++;
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string.");

    advance();
    return makeToken(TokenType::String);
}

Token Scanner::number() {
    while (std::isdigit(static_cast<unsigned char>(peek()))) advance();

    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
        advance();

        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
    }

    return makeToken(TokenType::Number);
}

Token Scanner::identifier() {
    while (
        std::isalnum(static_cast<unsigned char>(peek())) ||
        peek() == '_'
    ) {
        advance();
    }

    return makeToken(identifierType());
}

TokenType Scanner::identifierType() const {
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"and", TokenType::And},
        {"class", TokenType::Class},
        {"else", TokenType::Else},
        {"false", TokenType::False},
        {"for", TokenType::For},
        {"fun", TokenType::Fun},
        {"if", TokenType::If},
        {"nil", TokenType::Nil},
        {"or", TokenType::Or},
        {"print", TokenType::Print},
        {"return", TokenType::Return},
        {"super", TokenType::Super},
        {"this", TokenType::This},
        {"true", TokenType::True},
        {"var", TokenType::Var},
        {"while", TokenType::While},
    };

    std::string text = source_.substr(start_, current_ - start_);
    auto it = keywords.find(text);
    if (it != keywords.end()) return it->second;
    return TokenType::Identifier;
}

Token Scanner::scanToken() {
    skipWhitespace();

    start_ = current_;

    if (isAtEnd()) return makeToken(TokenType::Eof);

    char c = advance();

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        return identifier();
    }

    if (std::isdigit(static_cast<unsigned char>(c))) {
        return number();
    }

    switch (c) {
        case '(': return makeToken(TokenType::LeftParen);
        case ')': return makeToken(TokenType::RightParen);
        case '{': return makeToken(TokenType::LeftBrace);
        case '}': return makeToken(TokenType::RightBrace);
        case ';': return makeToken(TokenType::Semicolon);
        case ',': return makeToken(TokenType::Comma);
        case '.': return makeToken(TokenType::Dot);
        case '-': return makeToken(TokenType::Minus);
        case '+': return makeToken(TokenType::Plus);
        case '/': return makeToken(TokenType::Slash);
        case '*': return makeToken(TokenType::Star);

        case '!':
            return makeToken(match('=') ? TokenType::BangEqual : TokenType::Bang);

        case '=':
            return makeToken(match('=') ? TokenType::EqualEqual : TokenType::Equal);

        case '<':
            return makeToken(match('=') ? TokenType::LessEqual : TokenType::Less);

        case '>':
            return makeToken(match('=') ? TokenType::GreaterEqual : TokenType::Greater);

        case '"':
            return string();
    }

    return errorToken("Unexpected character.");
}