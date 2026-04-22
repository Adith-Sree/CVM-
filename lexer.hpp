#pragma once
#include "cvm_types.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace cvm {

class Lexer {
public:
    explicit Lexer(std::string source) : src_(std::move(source)) {}

    std::vector<Token> tokenize();

    // Return raw token list for debug
    const std::vector<Token>& tokens() const { return tokens_; }

private:
    std::string src_;
    size_t pos_  = 0;
    int    line_ = 1;
    int    col_  = 1;
    std::vector<Token> tokens_;

    static const std::unordered_map<std::string, TokenType> keywords_;

    char peek(int offset = 0) const {
        size_t idx = pos_ + offset;
        return idx < src_.size() ? src_[idx] : '\0';
    }
    char advance() {
        char c = src_[pos_++];
        if (c == '\n') { ++line_; col_ = 1; }
        else ++col_;
        return c;
    }
    bool match(char expected) {
        if (pos_ < src_.size() && src_[pos_] == expected) {
            advance(); return true;
        }
        return false;
    }
    void skip_whitespace_and_comments();
    Token read_number();
    Token read_string();
    Token read_identifier();
    void push(TokenType t, const std::string& lex) {
        tokens_.emplace_back(t, lex, line_, col_);
    }
};

} // namespace cvm
