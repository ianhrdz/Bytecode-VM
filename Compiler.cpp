// Compiler.cpp
#include "Compiler.h"

#include "Common.h"
#include "Debug.h"
#include "Scanner.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

enum class Precedence {
    None,
    Assignment,
    Or,
    And,
    Equality,
    Comparison,
    Term,
    Factor,
    Unary,
    Call,
    Primary,
};

enum class FunctionType {
    Function,
    Script,
};

struct Local {
    Token name;
    int depth = 0;
    bool isCaptured = false;
};

struct Upvalue {
    uint8_t index = 0;
    bool isLocal = false;
};

class Parser {
public:
    explicit Parser(std::string source)
        : scanner_(std::move(source)) {}

    Token current;
    Token previous;
    bool hadError = false;
    bool panicMode = false;

    void advance() {
        previous = current;

        for (;;) {
            current = scanner_.scanToken();
            if (current.type != TokenType::Error) break;

            errorAtCurrent(current.lexeme);
        }
    }

    bool check(TokenType type) const {
        return current.type == type;
    }

    bool match(TokenType type) {
        if (!check(type)) return false;
        advance();
        return true;
    }

    void consume(TokenType type, const std::string& message) {
        if (current.type == type) {
            advance();
            return;
        }

        errorAtCurrent(message);
    }

    void errorAtCurrent(const std::string& message) {
        errorAt(current, message);
    }

    void error(const std::string& message) {
        errorAt(previous, message);
    }

private:
    Scanner scanner_;

    void errorAt(const Token& token, const std::string& message) {
        if (panicMode) return;
        panicMode = true;

        std::cerr << "[line " << token.line << "] Error";

        if (token.type == TokenType::Eof) {
            std::cerr << " at end";
        } else if (token.type != TokenType::Error) {
            std::cerr << " at '" << token.lexeme << "'";
        }

        std::cerr << ": " << message << "\n";
        hadError = true;
    }
};

class CompilerImpl {
public:
    CompilerImpl(Parser& parser, FunctionType type, CompilerImpl* enclosing)
        : parser_(parser),
          function_(std::make_shared<ObjFunction>()),
          type_(type),
          enclosing_(enclosing) {
        if (type_ != FunctionType::Script) {
            function_->name = parser_.previous.lexeme;
        }

        Local local;
        local.depth = 0;
        local.name.lexeme = "";
        locals_.push_back(local);
    }

    std::shared_ptr<ObjFunction> compile() {
        parser_.advance();

        while (!parser_.match(TokenType::Eof)) {
            declaration();
        }

        return endCompiler();
    }

private:
    using ParseFn = void (CompilerImpl::*)(bool canAssign);

    struct ParseRule {
        ParseFn prefix;
        ParseFn infix;
        Precedence precedence;
    };

    Parser& parser_;
    std::shared_ptr<ObjFunction> function_;
    FunctionType type_;
    CompilerImpl* enclosing_;
    std::vector<Local> locals_;
    std::vector<Upvalue> upvalues_;
    int scopeDepth_ = 0;

    Chunk& currentChunk() {
        return function_->chunk;
    }

    void emitByte(uint8_t byte) {
        currentChunk().write(byte, parser_.previous.line);
    }

    void emitByte(OpCode op) {
        currentChunk().write(op, parser_.previous.line);
    }

    void emitBytes(OpCode op, uint8_t byte) {
        emitByte(op);
        emitByte(byte);
    }

    void emitBytes(OpCode op, OpCode op2) {
        emitByte(op);
        emitByte(op2);
    }

    void emitLoop(int loopStart) {
        emitByte(OpCode::Loop);

        int offset = static_cast<int>(currentChunk().code().size()) - loopStart + 2;
        if (offset > UINT16_MAX) {
            parser_.error("Loop body too large.");
        }

        emitByte(static_cast<uint8_t>((offset >> 8) & 0xff));
        emitByte(static_cast<uint8_t>(offset & 0xff));
    }

    int emitJump(OpCode instruction) {
        emitByte(instruction);
        emitByte(0xff);
        emitByte(0xff);
        return static_cast<int>(currentChunk().code().size()) - 2;
    }

    void emitReturn() {
        emitByte(OpCode::Nil);
        emitByte(OpCode::Return);
    }

    uint8_t makeConstant(const Value& value) {
        int constant = currentChunk().addConstant(value);

        if (constant > UINT8_MAX) {
            parser_.error("Too many constants in one chunk.");
            return 0;
        }

        return static_cast<uint8_t>(constant);
    }

    void emitConstant(const Value& value) {
        emitBytes(OpCode::Constant, makeConstant(value));
    }

    void patchJump(int offset) {
        int jump = static_cast<int>(currentChunk().code().size()) - offset - 2;

        if (jump > UINT16_MAX) {
            parser_.error("Too much code to jump over.");
        }

        currentChunk().code()[offset] = static_cast<uint8_t>((jump >> 8) & 0xff);
        currentChunk().code()[offset + 1] = static_cast<uint8_t>(jump & 0xff);
    }

    std::shared_ptr<ObjFunction> endCompiler() {
        emitReturn();

#if DEBUG_PRINT_CODE
        if (!parser_.hadError) {
            disassembleChunk(currentChunk(), function_->name.empty() ? "<script>" : function_->name);
        }
#endif

        return function_;
    }

    void beginScope() {
        scopeDepth_++;
    }

    void endScope() {
        scopeDepth_--;

        while (!locals_.empty() && locals_.back().depth > scopeDepth_) {
            if (locals_.back().isCaptured) {
                emitByte(OpCode::CloseUpvalue);
            } else {
                emitByte(OpCode::Pop);
            }

            locals_.pop_back();
        }
    }

    void declaration() {
        if (parser_.match(TokenType::Fun)) {
            funDeclaration();
        } else if (parser_.match(TokenType::Var)) {
            varDeclaration();
        } else {
            statement();
        }

        if (parser_.panicMode) synchronize();
    }

    void funDeclaration() {
        uint8_t global = parseVariable("Expect function name.");
        markInitialized();
        function();
        defineVariable(global);
    }

    void varDeclaration() {
        uint8_t global = parseVariable("Expect variable name.");

        if (parser_.match(TokenType::Equal)) {
            expression();
        } else {
            emitByte(OpCode::Nil);
        }

        parser_.consume(TokenType::Semicolon, "Expect ';' after variable declaration.");
        defineVariable(global);
    }

    void statement() {
        if (parser_.match(TokenType::Print)) {
            printStatement();
        } else if (parser_.match(TokenType::For)) {
            forStatement();
        } else if (parser_.match(TokenType::If)) {
            ifStatement();
        } else if (parser_.match(TokenType::Return)) {
            returnStatement();
        } else if (parser_.match(TokenType::While)) {
            whileStatement();
        } else if (parser_.match(TokenType::LeftBrace)) {
            beginScope();
            block();
            endScope();
        } else {
            expressionStatement();
        }
    }

    void printStatement() {
        expression();
        parser_.consume(TokenType::Semicolon, "Expect ';' after value.");
        emitByte(OpCode::Print);
    }

    void returnStatement() {
        if (type_ == FunctionType::Script) {
            parser_.error("Can't return from top-level code.");
        }

        if (parser_.match(TokenType::Semicolon)) {
            emitReturn();
        } else {
            expression();
            parser_.consume(TokenType::Semicolon, "Expect ';' after return value.");
            emitByte(OpCode::Return);
        }
    }

    void expressionStatement() {
        expression();
        parser_.consume(TokenType::Semicolon, "Expect ';' after expression.");
        emitByte(OpCode::Pop);
    }

    void block() {
        while (!parser_.check(TokenType::RightBrace) && !parser_.check(TokenType::Eof)) {
            declaration();
        }

        parser_.consume(TokenType::RightBrace, "Expect '}' after block.");
    }

    void ifStatement() {
        parser_.consume(TokenType::LeftParen, "Expect '(' after 'if'.");
        expression();
        parser_.consume(TokenType::RightParen, "Expect ')' after condition.");

        int thenJump = emitJump(OpCode::JumpIfFalse);
        emitByte(OpCode::Pop);
        statement();

        int elseJump = emitJump(OpCode::Jump);

        patchJump(thenJump);
        emitByte(OpCode::Pop);

        if (parser_.match(TokenType::Else)) {
            statement();
        }

        patchJump(elseJump);
    }

    void whileStatement() {
        int loopStart = static_cast<int>(currentChunk().code().size());

        parser_.consume(TokenType::LeftParen, "Expect '(' after 'while'.");
        expression();
        parser_.consume(TokenType::RightParen, "Expect ')' after condition.");

        int exitJump = emitJump(OpCode::JumpIfFalse);

        emitByte(OpCode::Pop);
        statement();
        emitLoop(loopStart);

        patchJump(exitJump);
        emitByte(OpCode::Pop);
    }

    void forStatement() {
        beginScope();

        parser_.consume(TokenType::LeftParen, "Expect '(' after 'for'.");

        if (parser_.match(TokenType::Semicolon)) {
            // No initializer.
        } else if (parser_.match(TokenType::Var)) {
            varDeclaration();
        } else {
            expressionStatement();
        }

        int loopStart = static_cast<int>(currentChunk().code().size());
        int exitJump = -1;

        if (!parser_.match(TokenType::Semicolon)) {
            expression();
            parser_.consume(TokenType::Semicolon, "Expect ';' after loop condition.");

            exitJump = emitJump(OpCode::JumpIfFalse);
            emitByte(OpCode::Pop);
        }

        if (!parser_.match(TokenType::RightParen)) {
            int bodyJump = emitJump(OpCode::Jump);

            int incrementStart = static_cast<int>(currentChunk().code().size());
            expression();
            emitByte(OpCode::Pop);

            parser_.consume(TokenType::RightParen, "Expect ')' after for clauses.");

            emitLoop(loopStart);
            loopStart = incrementStart;

            patchJump(bodyJump);
        }

        statement();
        emitLoop(loopStart);

        if (exitJump != -1) {
            patchJump(exitJump);
            emitByte(OpCode::Pop);
        }

        endScope();
    }

    void expression() {
        parsePrecedence(Precedence::Assignment);
    }

    void function() {
        CompilerImpl compiler(parser_, FunctionType::Function, this);
        compiler.beginScope();

        parser_.consume(TokenType::LeftParen, "Expect '(' after function name.");

        if (!parser_.check(TokenType::RightParen)) {
            do {
                compiler.function_->arity++;

                if (compiler.function_->arity > 255) {
                    parser_.errorAtCurrent("Can't have more than 255 parameters.");
                }

                uint8_t constant = compiler.parseVariable("Expect parameter name.");
                compiler.defineVariable(constant);
            } while (parser_.match(TokenType::Comma));
        }

        parser_.consume(TokenType::RightParen, "Expect ')' after parameters.");
        parser_.consume(TokenType::LeftBrace, "Expect '{' before function body.");

        compiler.block();

        std::shared_ptr<ObjFunction> function = compiler.endCompiler();

        emitBytes(
            OpCode::Closure,
            makeConstant(std::static_pointer_cast<Obj>(function))
        );

        for (int i = 0; i < function->upvalueCount; i++) {
            emitByte(compiler.upvalues_[i].isLocal ? 1 : 0);
            emitByte(compiler.upvalues_[i].index);
        }
    }

    void parsePrecedence(Precedence precedence) {
        parser_.advance();

        ParseFn prefixRule = getRule(parser_.previous.type).prefix;

        if (prefixRule == nullptr) {
            parser_.error("Expect expression.");
            return;
        }

        bool canAssign = precedence <= Precedence::Assignment;
        (this->*prefixRule)(canAssign);

        while (precedence <= getRule(parser_.current.type).precedence) {
            parser_.advance();
            ParseFn infixRule = getRule(parser_.previous.type).infix;
            (this->*infixRule)(canAssign);
        }

        if (canAssign && parser_.match(TokenType::Equal)) {
            parser_.error("Invalid assignment target.");
        }
    }

    void number(bool) {
        double value = std::strtod(parser_.previous.lexeme.c_str(), nullptr);
        emitConstant(value);
    }

    void grouping(bool) {
        expression();
        parser_.consume(TokenType::RightParen, "Expect ')' after expression.");
    }

    void unary(bool) {
        TokenType operatorType = parser_.previous.type;

        parsePrecedence(Precedence::Unary);

        switch (operatorType) {
            case TokenType::Bang:
                emitByte(OpCode::Not);
                break;
            case TokenType::Minus:
                emitByte(OpCode::Negate);
                break;
            default:
                return;
        }
    }

    void binary(bool) {
        TokenType operatorType = parser_.previous.type;
        ParseRule rule = getRule(operatorType);

        parsePrecedence(static_cast<Precedence>(static_cast<int>(rule.precedence) + 1));

        switch (operatorType) {
            case TokenType::BangEqual:
                emitBytes(OpCode::Equal, OpCode::Not);
                break;
            case TokenType::EqualEqual:
                emitByte(OpCode::Equal);
                break;
            case TokenType::Greater:
                emitByte(OpCode::Greater);
                break;
            case TokenType::GreaterEqual:
                emitBytes(OpCode::Less, OpCode::Not);
                break;
            case TokenType::Less:
                emitByte(OpCode::Less);
                break;
            case TokenType::LessEqual:
                emitBytes(OpCode::Greater, OpCode::Not);
                break;
            case TokenType::Plus:
                emitByte(OpCode::Add);
                break;
            case TokenType::Minus:
                emitByte(OpCode::Subtract);
                break;
            case TokenType::Star:
                emitByte(OpCode::Multiply);
                break;
            case TokenType::Slash:
                emitByte(OpCode::Divide);
                break;
            default:
                return;
        }
    }

    void literal(bool) {
        switch (parser_.previous.type) {
            case TokenType::False:
                emitByte(OpCode::False);
                break;
            case TokenType::Nil:
                emitByte(OpCode::Nil);
                break;
            case TokenType::True:
                emitByte(OpCode::True);
                break;
            default:
                return;
        }
    }

    void string(bool) {
        std::string raw = parser_.previous.lexeme;
        std::string value = raw.substr(1, raw.size() - 2);

        emitConstant(std::static_pointer_cast<Obj>(
            std::make_shared<ObjString>(value)
        ));
    }

    void variable(bool canAssign) {
        namedVariable(parser_.previous, canAssign);
    }

    void and_(bool) {
        int endJump = emitJump(OpCode::JumpIfFalse);
        emitByte(OpCode::Pop);
        parsePrecedence(Precedence::And);
        patchJump(endJump);
    }

    void or_(bool) {
        int elseJump = emitJump(OpCode::JumpIfFalse);
        int endJump = emitJump(OpCode::Jump);

        patchJump(elseJump);
        emitByte(OpCode::Pop);

        parsePrecedence(Precedence::Or);
        patchJump(endJump);
    }

    void call(bool) {
        uint8_t argCount = argumentList();
        emitBytes(OpCode::Call, argCount);
    }

    uint8_t argumentList() {
        uint8_t argCount = 0;

        if (!parser_.check(TokenType::RightParen)) {
            do {
                expression();

                if (argCount == 255) {
                    parser_.error("Can't have more than 255 arguments.");
                }

                argCount++;
            } while (parser_.match(TokenType::Comma));
        }

        parser_.consume(TokenType::RightParen, "Expect ')' after arguments.");
        return argCount;
    }

    void namedVariable(const Token& name, bool canAssign) {
        OpCode getOp;
        OpCode setOp;
        int arg = resolveLocal(name);

        if (arg != -1) {
            getOp = OpCode::GetLocal;
            setOp = OpCode::SetLocal;
        } else if ((arg = resolveUpvalue(name)) != -1) {
            getOp = OpCode::GetUpvalue;
            setOp = OpCode::SetUpvalue;
        } else {
            arg = identifierConstant(name);
            getOp = OpCode::GetGlobal;
            setOp = OpCode::SetGlobal;
        }

        if (canAssign && parser_.match(TokenType::Equal)) {
            expression();
            emitBytes(setOp, static_cast<uint8_t>(arg));
        } else {
            emitBytes(getOp, static_cast<uint8_t>(arg));
        }
    }

    uint8_t parseVariable(const std::string& errorMessage) {
        parser_.consume(TokenType::Identifier, errorMessage);

        declareVariable();

        if (scopeDepth_ > 0) return 0;

        return identifierConstant(parser_.previous);
    }

    uint8_t identifierConstant(const Token& name) {
        return makeConstant(std::static_pointer_cast<Obj>(
            std::make_shared<ObjString>(name.lexeme)
        ));
    }

    bool identifiersEqual(const Token& a, const Token& b) {
        return a.lexeme == b.lexeme;
    }

    void declareVariable() {
        if (scopeDepth_ == 0) return;

        const Token& name = parser_.previous;

        for (auto it = locals_.rbegin(); it != locals_.rend(); ++it) {
            if (it->depth != -1 && it->depth < scopeDepth_) break;

            if (identifiersEqual(name, it->name)) {
                parser_.error("Already a variable with this name in this scope.");
            }
        }

        addLocal(name);
    }

    void addLocal(const Token& name) {
        if (locals_.size() == UINT8_MAX + 1) {
            parser_.error("Too many local variables in function.");
            return;
        }

        Local local;
        local.name = name;
        local.depth = -1;
        locals_.push_back(local);
    }

    void defineVariable(uint8_t global) {
        if (scopeDepth_ > 0) {
            markInitialized();
            return;
        }

        emitBytes(OpCode::DefineGlobal, global);
    }

    void markInitialized() {
        if (scopeDepth_ == 0) return;
        locals_.back().depth = scopeDepth_;
    }

    int resolveLocal(const Token& name) {
        for (int i = static_cast<int>(locals_.size()) - 1; i >= 0; i--) {
            if (identifiersEqual(name, locals_[i].name)) {
                if (locals_[i].depth == -1) {
                    parser_.error("Can't read local variable in its own initializer.");
                }

                return i;
            }
        }

        return -1;
    }

    int resolveUpvalue(const Token& name) {
        if (enclosing_ == nullptr) return -1;

        int local = enclosing_->resolveLocal(name);

        if (local != -1) {
            enclosing_->locals_[local].isCaptured = true;
            return addUpvalue(static_cast<uint8_t>(local), true);
        }

        int upvalue = enclosing_->resolveUpvalue(name);

        if (upvalue != -1) {
            return addUpvalue(static_cast<uint8_t>(upvalue), false);
        }

        return -1;
    }

    int addUpvalue(uint8_t index, bool isLocal) {
        for (int i = 0; i < static_cast<int>(upvalues_.size()); i++) {
            Upvalue& upvalue = upvalues_[i];

            if (upvalue.index == index && upvalue.isLocal == isLocal) {
                return i;
            }
        }

        if (upvalues_.size() == UINT8_MAX + 1) {
            parser_.error("Too many closure variables in function.");
            return 0;
        }

        Upvalue upvalue;
        upvalue.isLocal = isLocal;
        upvalue.index = index;
        upvalues_.push_back(upvalue);

        function_->upvalueCount = static_cast<int>(upvalues_.size());
        return static_cast<int>(upvalues_.size() - 1);
    }

    void synchronize() {
        parser_.panicMode = false;

        while (parser_.current.type != TokenType::Eof) {
            if (parser_.previous.type == TokenType::Semicolon) return;

            switch (parser_.current.type) {
                case TokenType::Class:
                case TokenType::Fun:
                case TokenType::Var:
                case TokenType::For:
                case TokenType::If:
                case TokenType::While:
                case TokenType::Print:
                case TokenType::Return:
                    return;
                default:
                    break;
            }

            parser_.advance();
        }
    }

    ParseRule getRule(TokenType type) {
        switch (type) {
            case TokenType::LeftParen:
                return {&CompilerImpl::grouping, &CompilerImpl::call, Precedence::Call};
            case TokenType::RightParen:
                return {nullptr, nullptr, Precedence::None};
            case TokenType::LeftBrace:
            case TokenType::RightBrace:
            case TokenType::Comma:
            case TokenType::Dot:
            case TokenType::Semicolon:
                return {nullptr, nullptr, Precedence::None};

            case TokenType::Minus:
                return {&CompilerImpl::unary, &CompilerImpl::binary, Precedence::Term};
            case TokenType::Plus:
                return {nullptr, &CompilerImpl::binary, Precedence::Term};
            case TokenType::Slash:
            case TokenType::Star:
                return {nullptr, &CompilerImpl::binary, Precedence::Factor};

            case TokenType::Bang:
                return {&CompilerImpl::unary, nullptr, Precedence::None};
            case TokenType::BangEqual:
            case TokenType::EqualEqual:
                return {nullptr, &CompilerImpl::binary, Precedence::Equality};

            case TokenType::Equal:
                return {nullptr, nullptr, Precedence::None};

            case TokenType::Greater:
            case TokenType::GreaterEqual:
            case TokenType::Less:
            case TokenType::LessEqual:
                return {nullptr, &CompilerImpl::binary, Precedence::Comparison};

            case TokenType::Identifier:
                return {&CompilerImpl::variable, nullptr, Precedence::None};
            case TokenType::String:
                return {&CompilerImpl::string, nullptr, Precedence::None};
            case TokenType::Number:
                return {&CompilerImpl::number, nullptr, Precedence::None};

            case TokenType::And:
                return {nullptr, &CompilerImpl::and_, Precedence::And};
            case TokenType::Or:
                return {nullptr, &CompilerImpl::or_, Precedence::Or};

            case TokenType::False:
            case TokenType::Nil:
            case TokenType::True:
                return {&CompilerImpl::literal, nullptr, Precedence::None};

            case TokenType::Class:
            case TokenType::Else:
            case TokenType::For:
            case TokenType::Fun:
            case TokenType::If:
            case TokenType::Print:
            case TokenType::Return:
            case TokenType::Super:
            case TokenType::This:
            case TokenType::Var:
            case TokenType::While:
            case TokenType::Error:
            case TokenType::Eof:
                return {nullptr, nullptr, Precedence::None};
        }

        return {nullptr, nullptr, Precedence::None};
    }
};

std::shared_ptr<ObjFunction> compile(const std::string& source) {
    Parser parser(source);
    CompilerImpl compiler(parser, FunctionType::Script, nullptr);

    std::shared_ptr<ObjFunction> function = compiler.compile();

    if (parser.hadError) return nullptr;
    return function;
}