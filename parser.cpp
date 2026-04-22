#include "parser.hpp"
#include <stdexcept>
#include <sstream>

namespace cvm {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

// ─── Token Navigation ───────────────────────
const Token& Parser::peek(int offset) const {
    size_t idx = pos_ + offset;
    if (idx >= tokens_.size()) return tokens_.back();
    return tokens_[idx];
}
const Token& Parser::advance() {
    if (!at_end()) ++pos_;
    return tokens_[pos_ - 1];
}
bool Parser::check(TokenType t) const { return !at_end() && peek().type == t; }
bool Parser::match(TokenType t) {
    if (check(t)) { advance(); return true; }
    return false;
}
bool Parser::match(std::initializer_list<TokenType> types) {
    for (auto t : types) if (check(t)) { advance(); return true; }
    return false;
}
const Token& Parser::expect(TokenType t, const std::string& msg) {
    if (!check(t)) {
        auto& tok = peek();
        throw ParseError(msg + " (got '" + tok.lexeme + "')", tok.line, tok.col);
    }
    return advance();
}
void Parser::skip_newlines() {
    while (check(TokenType::NEWLINE) || check(TokenType::SEMICOLON)) advance();
}
bool Parser::at_end() const {
    return pos_ >= tokens_.size() || tokens_[pos_].type == TokenType::EOF_TOKEN;
}

// ─── Entry ──────────────────────────────────
ASTNodePtr Parser::parse() { return parse_program(); }

ASTNodePtr Parser::parse_program() {
    std::vector<ASTNodePtr> stmts;
    skip_newlines();
    while (!at_end()) {
        stmts.push_back(parse_statement());
        skip_newlines();
    }
    auto prog = std::make_unique<ProgramNode>(std::move(stmts));
    prog->line = 1;
    return prog;
}

// ─── Statements ─────────────────────────────
ASTNodePtr Parser::parse_statement() {
    skip_newlines();
    auto& tok = peek();

    if (tok.type == TokenType::LET)    return parse_let_stmt();
    if (tok.type == TokenType::IF)     return parse_if_stmt();
    if (tok.type == TokenType::WHILE)  return parse_while_stmt();
    if (tok.type == TokenType::FOR)    return parse_for_stmt();
    if (tok.type == TokenType::FN)     return parse_fn_stmt();
    if (tok.type == TokenType::RETURN) return parse_return_stmt();
    if (tok.type == TokenType::PRINT)  return parse_print_stmt();
    if (tok.type == TokenType::LBRACE) return parse_block();
    if (tok.type == TokenType::BREAK) {
        advance();
        auto node = std::make_unique<BreakStmt>();
        node->line = tok.line;
        return node;
    }
    if (tok.type == TokenType::CONTINUE) {
        advance();
        auto node = std::make_unique<ContinueStmt>();
        node->line = tok.line;
        return node;
    }
    return parse_expr_stmt();
}

ASTNodePtr Parser::parse_let_stmt() {
    int ln = peek().line;
    advance(); // consume 'let'
    auto& name_tok = expect(TokenType::IDENTIFIER, "Expected variable name after 'let'");
    std::string name = name_tok.lexeme;

    ASTNodePtr init = nullptr;
    if (match(TokenType::ASSIGN)) {
        init = parse_expr();
    }
    auto node = std::make_unique<LetStmt>(name, std::move(init));
    node->line = ln;
    return node;
}

ASTNodePtr Parser::parse_if_stmt() {
    int ln = peek().line;
    advance(); // 'if'
    expect(TokenType::LPAREN, "Expected '(' after 'if'");
    auto cond = parse_expr();
    expect(TokenType::RPAREN, "Expected ')' after if condition");
    auto then_br = parse_statement();
    ASTNodePtr else_br = nullptr;
    skip_newlines();
    if (check(TokenType::ELSE)) {
        advance();
        skip_newlines();
        else_br = parse_statement();
    }
    auto node = std::make_unique<IfStmt>(std::move(cond), std::move(then_br), std::move(else_br));
    node->line = ln;
    return node;
}

ASTNodePtr Parser::parse_while_stmt() {
    int ln = peek().line;
    advance(); // 'while'
    expect(TokenType::LPAREN, "Expected '(' after 'while'");
    auto cond = parse_expr();
    expect(TokenType::RPAREN, "Expected ')' after while condition");
    auto body = parse_statement();
    auto node = std::make_unique<WhileStmt>(std::move(cond), std::move(body));
    node->line = ln;
    return node;
}

ASTNodePtr Parser::parse_for_stmt() {
    int ln = peek().line;
    advance(); // 'for'
    expect(TokenType::LPAREN, "Expected '(' after 'for'");

    ASTNodePtr init = nullptr;
    if (!check(TokenType::SEMICOLON) && !check(TokenType::NEWLINE)) {
        if (check(TokenType::LET)) init = parse_let_stmt();
        else init = parse_expr_stmt();
    }
    match({TokenType::SEMICOLON, TokenType::NEWLINE});

    ASTNodePtr cond = nullptr;
    if (!check(TokenType::SEMICOLON) && !check(TokenType::NEWLINE))
        cond = parse_expr();
    match({TokenType::SEMICOLON, TokenType::NEWLINE});

    ASTNodePtr inc = nullptr;
    if (!check(TokenType::RPAREN))
        inc = parse_expr();

    expect(TokenType::RPAREN, "Expected ')' after for clauses");
    auto body = parse_statement();
    auto node = std::make_unique<ForStmt>(std::move(init), std::move(cond), std::move(inc), std::move(body));
    node->line = ln;
    return node;
}

ASTNodePtr Parser::parse_fn_stmt() {
    int ln = peek().line;
    advance(); // 'fn'
    auto& name_tok = expect(TokenType::IDENTIFIER, "Expected function name");
    std::string name = name_tok.lexeme;
    expect(TokenType::LPAREN, "Expected '(' after function name");

    std::vector<std::string> params;
    if (!check(TokenType::RPAREN)) {
        do {
            auto& p = expect(TokenType::IDENTIFIER, "Expected parameter name");
            params.push_back(p.lexeme);
        } while (match(TokenType::COMMA));
    }
    expect(TokenType::RPAREN, "Expected ')' after parameters");
    skip_newlines();
    auto body = parse_block();
    auto node = std::make_unique<FnStmt>(name, std::move(params), std::move(body));
    node->line = ln;
    return node;
}

ASTNodePtr Parser::parse_return_stmt() {
    int ln = peek().line;
    advance(); // 'return'
    ASTNodePtr val = nullptr;
    if (!check(TokenType::NEWLINE) && !check(TokenType::SEMICOLON) && !check(TokenType::EOF_TOKEN))
        val = parse_expr();
    auto node = std::make_unique<ReturnStmt>(std::move(val));
    node->line = ln;
    return node;
}

ASTNodePtr Parser::parse_print_stmt() {
    int ln = peek().line;
    advance(); // 'print'
    expect(TokenType::LPAREN, "Expected '(' after 'print'");
    std::vector<ASTNodePtr> exprs;
    if (!check(TokenType::RPAREN)) {
        exprs.push_back(parse_expr());
        while (match(TokenType::COMMA)) exprs.push_back(parse_expr());
    }
    expect(TokenType::RPAREN, "Expected ')' after print arguments");
    auto node = std::make_unique<PrintStmt>(std::move(exprs));
    node->line = ln;
    return node;
}

ASTNodePtr Parser::parse_block() {
    int ln = peek().line;
    expect(TokenType::LBRACE, "Expected '{'");
    std::vector<ASTNodePtr> stmts;
    skip_newlines();
    while (!check(TokenType::RBRACE) && !at_end()) {
        stmts.push_back(parse_statement());
        skip_newlines();
    }
    expect(TokenType::RBRACE, "Expected '}'");
    auto node = std::make_unique<BlockStmt>(std::move(stmts));
    node->line = ln;
    return node;
}

ASTNodePtr Parser::parse_expr_stmt() {
    auto expr = parse_expr();
    auto node = std::make_unique<ExprStmt>(std::move(expr));
    return node;
}

// ─── Expressions (Recursive Descent) ────────

ASTNodePtr Parser::parse_expr() { return parse_assign(); }

ASTNodePtr Parser::parse_assign() {
    int ln = peek().line;
    // Lookahead for compound assignment
    if (check(TokenType::IDENTIFIER)) {
        std::string name = peek().lexeme;
        auto nxt = peek(1).type;
        if (nxt == TokenType::ASSIGN) {
            advance(); advance(); // name, =
            auto val = parse_expr();
            auto node = std::make_unique<AssignExpr>(name, std::move(val));
            node->line = ln;
            return node;
        }
        // Compound assignments: +=, -=, *=, /=
        if (nxt == TokenType::PLUS_ASSIGN  || nxt == TokenType::MINUS_ASSIGN ||
            nxt == TokenType::STAR_ASSIGN  || nxt == TokenType::SLASH_ASSIGN) {
            advance();
            std::string op = advance().lexeme;
            auto val = parse_expr();
            auto node = std::make_unique<CompoundAssignExpr>(name, op, std::move(val));
            node->line = ln;
            return node;
        }
    }
    return parse_or();
}

ASTNodePtr Parser::parse_or() {
    auto left = parse_and();
    while (check(TokenType::OR)) {
        int ln = peek().line;
        advance();
        auto right = parse_and();
        auto node = std::make_unique<BinaryExpr>("or", std::move(left), std::move(right));
        node->line = ln;
        left = std::move(node);
    }
    return left;
}

ASTNodePtr Parser::parse_and() {
    auto left = parse_equality();
    while (check(TokenType::AND)) {
        int ln = peek().line;
        advance();
        auto right = parse_equality();
        auto node = std::make_unique<BinaryExpr>("and", std::move(left), std::move(right));
        node->line = ln;
        left = std::move(node);
    }
    return left;
}

ASTNodePtr Parser::parse_equality() {
    auto left = parse_comparison();
    while (check(TokenType::EQ) || check(TokenType::NEQ)) {
        int ln = peek().line;
        std::string op = advance().lexeme;
        auto right = parse_comparison();
        auto node = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
        node->line = ln;
        left = std::move(node);
    }
    return left;
}

ASTNodePtr Parser::parse_comparison() {
    auto left = parse_addition();
    while (check(TokenType::LT) || check(TokenType::GT) ||
           check(TokenType::LTE) || check(TokenType::GTE)) {
        int ln = peek().line;
        std::string op = advance().lexeme;
        auto right = parse_addition();
        auto node = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
        node->line = ln;
        left = std::move(node);
    }
    return left;
}

ASTNodePtr Parser::parse_addition() {
    auto left = parse_multiplication();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        int ln = peek().line;
        std::string op = advance().lexeme;
        auto right = parse_multiplication();
        auto node = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
        node->line = ln;
        left = std::move(node);
    }
    return left;
}

ASTNodePtr Parser::parse_multiplication() {
    auto left = parse_unary();
    while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::PERCENT)) {
        int ln = peek().line;
        std::string op = advance().lexeme;
        auto right = parse_unary();
        auto node = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
        node->line = ln;
        left = std::move(node);
    }
    return left;
}

ASTNodePtr Parser::parse_unary() {
    if (check(TokenType::MINUS) || check(TokenType::NOT)) {
        int ln = peek().line;
        std::string op = advance().lexeme;
        auto operand = parse_unary();
        auto node = std::make_unique<UnaryExpr>(op, std::move(operand));
        node->line = ln;
        return node;
    }
    // Prefix ++ / --
    if (check(TokenType::INCREMENT) || check(TokenType::DECREMENT)) {
        int ln = peek().line;
        std::string op = advance().lexeme;
        auto operand = parse_unary();
        // Desugar ++x → x = x + 1
        auto name_node = dynamic_cast<IdentifierExpr*>(operand.get());
        if (!name_node) throw ParseError("Expected identifier after " + op, ln, 0);
        std::string name = name_node->name;
        auto one = std::make_unique<LiteralExpr>(Value(int64_t(1)));
        auto bin_op = (op == "++") ? "+" : "-";
        auto ident  = std::make_unique<IdentifierExpr>(name);
        auto binex  = std::make_unique<BinaryExpr>(bin_op, std::move(ident), std::move(one));
        auto node   = std::make_unique<AssignExpr>(name, std::move(binex));
        node->line = ln;
        return node;
    }
    return parse_primary();
}

ASTNodePtr Parser::parse_primary() {
    auto& tok = peek();
    int ln = tok.line, cl = tok.col;

    if (tok.type == TokenType::NUMBER) {
        advance();
        auto node = std::make_unique<LiteralExpr>(Value(int64_t(std::stoll(tok.lexeme))));
        node->line = ln; node->col = cl;
        return node;
    }
    if (tok.type == TokenType::FLOAT) {
        advance();
        auto node = std::make_unique<LiteralExpr>(Value(std::stod(tok.lexeme)));
        node->line = ln; node->col = cl;
        return node;
    }
    if (tok.type == TokenType::STRING) {
        advance();
        auto node = std::make_unique<LiteralExpr>(Value(tok.lexeme));
        node->line = ln; node->col = cl;
        return node;
    }
    if (tok.type == TokenType::BOOL_TRUE) {
        advance();
        auto node = std::make_unique<LiteralExpr>(Value(true));
        node->line = ln; node->col = cl;
        return node;
    }
    if (tok.type == TokenType::BOOL_FALSE) {
        advance();
        auto node = std::make_unique<LiteralExpr>(Value(false));
        node->line = ln; node->col = cl;
        return node;
    }
    if (tok.type == TokenType::NIL) {
        advance();
        auto node = std::make_unique<LiteralExpr>(Value(std::monostate{}));
        node->line = ln; node->col = cl;
        return node;
    }
    if (tok.type == TokenType::INPUT) {
        advance(); // consume 'input'
        ASTNodePtr prompt = nullptr;
        if (check(TokenType::LPAREN)) {
            advance();
            if (!check(TokenType::RPAREN)) prompt = parse_expr();
            expect(TokenType::RPAREN, "Expected ')' after input prompt");
        }
        auto node = std::make_unique<InputExpr>(std::move(prompt));
        node->line = ln; node->col = cl;
        return node;
    }
    if (tok.type == TokenType::IDENTIFIER) {
        advance();
        // Function call?
        if (check(TokenType::LPAREN)) {
            auto ident = std::make_unique<IdentifierExpr>(tok.lexeme);
            ident->line = ln; ident->col = cl;
            return parse_call(std::move(ident));
        }
        // Postfix ++ / --
        if (check(TokenType::INCREMENT) || check(TokenType::DECREMENT)) {
            std::string op = advance().lexeme;
            std::string name = tok.lexeme;
            auto one  = std::make_unique<LiteralExpr>(Value(int64_t(1)));
            auto ident = std::make_unique<IdentifierExpr>(name);
            auto bin_op = (op == "++") ? "+" : "-";
            auto binex = std::make_unique<BinaryExpr>(bin_op, std::move(ident), std::move(one));
            // Returns old value (simplified: we just assign)
            auto node = std::make_unique<AssignExpr>(name, std::move(binex));
            node->line = ln;
            return node;
        }
        auto node = std::make_unique<IdentifierExpr>(tok.lexeme);
        node->line = ln; node->col = cl;
        return node;
    }
    if (tok.type == TokenType::LPAREN) {
        advance();
        auto expr = parse_expr();
        expect(TokenType::RPAREN, "Expected ')'");
        return expr;
    }
    throw ParseError("Unexpected token '" + tok.lexeme + "'", tok.line, tok.col);
}

ASTNodePtr Parser::parse_call(ASTNodePtr callee) {
    auto* ident = dynamic_cast<IdentifierExpr*>(callee.get());
    if (!ident) throw ParseError("Can only call identifiers", 0, 0);
    std::string name = ident->name;
    int ln = ident->line;

    advance(); // consume '('
    std::vector<ASTNodePtr> args;
    if (!check(TokenType::RPAREN)) {
        args.push_back(parse_expr());
        while (match(TokenType::COMMA)) args.push_back(parse_expr());
    }
    expect(TokenType::RPAREN, "Expected ')' after arguments");
    auto node = std::make_unique<CallExpr>(name, std::move(args));
    node->line = ln;
    return node;
}

} // namespace cvm
