#pragma once
#include "cvm_types.hpp"
#include "ast.hpp"
#include <vector>
#include <memory>

namespace cvm {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    ASTNodePtr parse();

private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;

    // Token navigation
    const Token& peek(int offset = 0) const;
    const Token& advance();
    bool check(TokenType t) const;
    bool match(TokenType t);
    bool match(std::initializer_list<TokenType> types);
    const Token& expect(TokenType t, const std::string& msg);
    void skip_newlines();
    bool at_end() const;

    // Grammar productions
    ASTNodePtr parse_program();
    ASTNodePtr parse_statement();
    ASTNodePtr parse_let_stmt();
    ASTNodePtr parse_if_stmt();
    ASTNodePtr parse_while_stmt();
    ASTNodePtr parse_for_stmt();
    ASTNodePtr parse_fn_stmt();
    ASTNodePtr parse_return_stmt();
    ASTNodePtr parse_print_stmt();
    ASTNodePtr parse_block();
    ASTNodePtr parse_expr_stmt();

    // Expressions (Pratt-style precedence)
    ASTNodePtr parse_expr();
    ASTNodePtr parse_assign();
    ASTNodePtr parse_or();
    ASTNodePtr parse_and();
    ASTNodePtr parse_equality();
    ASTNodePtr parse_comparison();
    ASTNodePtr parse_addition();
    ASTNodePtr parse_multiplication();
    ASTNodePtr parse_unary();
    ASTNodePtr parse_primary();
    ASTNodePtr parse_call(ASTNodePtr callee);
};

} // namespace cvm
