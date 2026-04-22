#include "lexer.hpp"
#include <cctype>
#include <stdexcept>

namespace cvm {

const std::unordered_map<std::string, TokenType> Lexer::keywords_ = {
    {"let",      TokenType::LET},
    {"if",       TokenType::IF},
    {"else",     TokenType::ELSE},
    {"while",    TokenType::WHILE},
    {"for",      TokenType::FOR},
    {"fn",       TokenType::FN},
    {"return",   TokenType::RETURN},
    {"print",    TokenType::PRINT},
    {"input",    TokenType::INPUT},
    {"true",     TokenType::BOOL_TRUE},
    {"false",    TokenType::BOOL_FALSE},
    {"nil",      TokenType::NIL},
    {"and",      TokenType::AND},
    {"or",       TokenType::OR},
    {"not",      TokenType::NOT},
    {"break",    TokenType::BREAK},
    {"continue", TokenType::CONTINUE},
};

void Lexer::skip_whitespace_and_comments() {
    while (pos_ < src_.size()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') { advance(); continue; }
        // Single-line comment
        if (c == '#') {
            while (pos_ < src_.size() && peek() != '\n') advance();
            continue;
        }
        // Multi-line comment /* ... */
        if (c == '/' && peek(1) == '*') {
            advance(); advance(); // consume /*
            while (pos_ < src_.size()) {
                if (peek() == '*' && peek(1) == '/') {
                    advance(); advance(); break;
                }
                advance();
            }
            continue;
        }
        break;
    }
}

Token Lexer::read_number() {
    int start_line = line_, start_col = col_;
    std::string num;
    bool is_float = false;

    while (pos_ < src_.size() && (std::isdigit(peek()) || peek() == '_'))
        if (peek() != '_') num += advance(); else advance();

    if (peek() == '.' && std::isdigit(peek(1))) {
        is_float = true;
        num += advance(); // consume '.'
        while (pos_ < src_.size() && std::isdigit(peek()))
            num += advance();
    }
    // Scientific notation
    if (peek() == 'e' || peek() == 'E') {
        is_float = true;
        num += advance();
        if (peek() == '+' || peek() == '-') num += advance();
        while (pos_ < src_.size() && std::isdigit(peek())) num += advance();
    }
    return Token(is_float ? TokenType::FLOAT : TokenType::NUMBER, num, start_line, start_col);
}

Token Lexer::read_string() {
    int start_line = line_, start_col = col_;
    char quote = advance(); // consume opening quote
    std::string s;
    while (pos_ < src_.size() && peek() != quote) {
        char c = advance();
        if (c == '\\') {
            char esc = advance();
            switch (esc) {
                case 'n': s += '\n'; break;
                case 't': s += '\t'; break;
                case 'r': s += '\r'; break;
                case '\\': s += '\\'; break;
                case '"':  s += '"';  break;
                case '\'': s += '\''; break;
                default:   s += '\\'; s += esc; break;
            }
        } else {
            s += c;
        }
    }
    if (pos_ >= src_.size())
        throw LexError("Unterminated string literal", start_line, start_col);
    advance(); // closing quote
    return Token(TokenType::STRING, s, start_line, start_col);
}

Token Lexer::read_identifier() {
    int start_line = line_, start_col = col_;
    std::string ident;
    while (pos_ < src_.size() && (std::isalnum(peek()) || peek() == '_'))
        ident += advance();
    auto it = keywords_.find(ident);
    TokenType type = (it != keywords_.end()) ? it->second : TokenType::IDENTIFIER;
    return Token(type, ident, start_line, start_col);
}

std::vector<Token> Lexer::tokenize() {
    tokens_.clear();

    while (pos_ < src_.size()) {
        skip_whitespace_and_comments();
        if (pos_ >= src_.size()) break;

        int ln = line_, cl = col_;
        char c = peek();

        // Newline (treated as statement terminator)
        if (c == '\n') {
            advance();
            tokens_.emplace_back(TokenType::NEWLINE, "\\n", ln, cl);
            continue;
        }

        // Numbers
        if (std::isdigit(c)) { tokens_.push_back(read_number()); continue; }

        // Strings
        if (c == '"' || c == '\'') { tokens_.push_back(read_string()); continue; }

        // Identifiers / keywords
        if (std::isalpha(c) || c == '_') { tokens_.push_back(read_identifier()); continue; }

        // Operators & punctuation
        advance();
        switch (c) {
            case '+':
                if (match('+')) tokens_.emplace_back(TokenType::INCREMENT, "++", ln, cl);
                else if (match('=')) tokens_.emplace_back(TokenType::PLUS_ASSIGN, "+=", ln, cl);
                else tokens_.emplace_back(TokenType::PLUS, "+", ln, cl);
                break;
            case '-':
                if (match('-')) tokens_.emplace_back(TokenType::DECREMENT, "--", ln, cl);
                else if (match('=')) tokens_.emplace_back(TokenType::MINUS_ASSIGN, "-=", ln, cl);
                else tokens_.emplace_back(TokenType::MINUS, "-", ln, cl);
                break;
            case '*':
                if (match('=')) tokens_.emplace_back(TokenType::STAR_ASSIGN, "*=", ln, cl);
                else tokens_.emplace_back(TokenType::STAR, "*", ln, cl);
                break;
            case '/':
                if (match('=')) tokens_.emplace_back(TokenType::SLASH_ASSIGN, "/=", ln, cl);
                else tokens_.emplace_back(TokenType::SLASH, "/", ln, cl);
                break;
            case '%': tokens_.emplace_back(TokenType::PERCENT, "%", ln, cl); break;
            case '=':
                if (match('=')) tokens_.emplace_back(TokenType::EQ, "==", ln, cl);
                else tokens_.emplace_back(TokenType::ASSIGN, "=", ln, cl);
                break;
            case '!':
                if (match('=')) tokens_.emplace_back(TokenType::NEQ, "!=", ln, cl);
                else throw LexError(std::string("Unexpected character '!'"), ln, cl);
                break;
            case '<':
                if (match('=')) tokens_.emplace_back(TokenType::LTE, "<=", ln, cl);
                else tokens_.emplace_back(TokenType::LT, "<", ln, cl);
                break;
            case '>':
                if (match('=')) tokens_.emplace_back(TokenType::GTE, ">=", ln, cl);
                else tokens_.emplace_back(TokenType::GT, ">", ln, cl);
                break;
            case '(': tokens_.emplace_back(TokenType::LPAREN,   "(", ln, cl); break;
            case ')': tokens_.emplace_back(TokenType::RPAREN,   ")", ln, cl); break;
            case '{': tokens_.emplace_back(TokenType::LBRACE,   "{", ln, cl); break;
            case '}': tokens_.emplace_back(TokenType::RBRACE,   "}", ln, cl); break;
            case '[': tokens_.emplace_back(TokenType::LBRACKET, "[", ln, cl); break;
            case ']': tokens_.emplace_back(TokenType::RBRACKET, "]", ln, cl); break;
            case ',': tokens_.emplace_back(TokenType::COMMA,    ",", ln, cl); break;
            case ';': tokens_.emplace_back(TokenType::SEMICOLON,";", ln, cl); break;
            case ':': tokens_.emplace_back(TokenType::COLON,    ":", ln, cl); break;
            case '.': tokens_.emplace_back(TokenType::DOT,      ".", ln, cl); break;
            default:
                throw LexError("Unexpected character '" + std::string(1, c) + "'", ln, cl);
        }
    }

    tokens_.emplace_back(TokenType::EOF_TOKEN, "", line_, col_);
    return tokens_;
}

} // namespace cvm
