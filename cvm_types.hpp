#pragma once
#include <string>
#include <variant>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <cstring>

// ─────────────────────────────────────────────
//  CVM++ Type System
// ─────────────────────────────────────────────
namespace cvm {

// Runtime value: int | bool | double | string | nil
using Value = std::variant<std::monostate, int64_t, bool, double, std::string>;

inline std::string value_to_string(const Value& v) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) return "nil";
        else if constexpr (std::is_same_v<T, int64_t>)  return std::to_string(arg);
        else if constexpr (std::is_same_v<T, bool>)     return arg ? "true" : "false";
        else if constexpr (std::is_same_v<T, double>) {
            std::string s = std::to_string(arg);
            // strip trailing zeros
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (s.back() == '.') s += '0';
            return s;
        }
        else if constexpr (std::is_same_v<T, std::string>) return arg;
    }, v);
}

inline bool value_truthy(const Value& v) {
    return std::visit([](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) return false;
        else if constexpr (std::is_same_v<T, bool>)     return arg;
        else if constexpr (std::is_same_v<T, int64_t>)  return arg != 0;
        else if constexpr (std::is_same_v<T, double>)   return arg != 0.0;
        else if constexpr (std::is_same_v<T, std::string>) return !arg.empty();
    }, v);
}

inline bool value_equal(const Value& a, const Value& b) { return a == b; }
inline bool value_less(const Value& a, const Value& b) {
    if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b))
        return std::get<int64_t>(a) < std::get<int64_t>(b);
    if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b))
        return std::get<double>(a) < std::get<double>(b);
    if (std::holds_alternative<int64_t>(a) && std::holds_alternative<double>(b))
        return (double)std::get<int64_t>(a) < std::get<double>(b);
    if (std::holds_alternative<double>(a) && std::holds_alternative<int64_t>(b))
        return std::get<double>(a) < (double)std::get<int64_t>(b);
    if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b))
        return std::get<std::string>(a) < std::get<std::string>(b);
    throw std::runtime_error("Cannot compare values of different types");
}

// ─────────────────────────────────────────────
//  Token Types
// ─────────────────────────────────────────────
enum class TokenType {
    // Literals
    NUMBER, FLOAT, STRING, BOOL_TRUE, BOOL_FALSE, NIL,
    // Identifiers & Keywords
    IDENTIFIER,
    LET, IF, ELSE, WHILE, FOR, FN, RETURN, PRINT, INPUT,
    AND, OR, NOT, BREAK, CONTINUE,
    // Operators
    PLUS, MINUS, STAR, SLASH, PERCENT,
    EQ, NEQ, LT, GT, LTE, GTE,
    ASSIGN, PLUS_ASSIGN, MINUS_ASSIGN, STAR_ASSIGN, SLASH_ASSIGN,
    INCREMENT, DECREMENT,
    // Delimiters
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    COMMA, SEMICOLON, COLON, DOT,
    // Special
    NEWLINE, EOF_TOKEN, UNKNOWN
};

inline std::string token_type_name(TokenType t) {
    switch(t) {
        case TokenType::NUMBER:      return "NUMBER";
        case TokenType::FLOAT:       return "FLOAT";
        case TokenType::STRING:      return "STRING";
        case TokenType::BOOL_TRUE:   return "TRUE";
        case TokenType::BOOL_FALSE:  return "FALSE";
        case TokenType::NIL:         return "NIL";
        case TokenType::IDENTIFIER:  return "IDENTIFIER";
        case TokenType::LET:         return "LET";
        case TokenType::IF:          return "IF";
        case TokenType::ELSE:        return "ELSE";
        case TokenType::WHILE:       return "WHILE";
        case TokenType::FOR:         return "FOR";
        case TokenType::FN:          return "FN";
        case TokenType::RETURN:      return "RETURN";
        case TokenType::PRINT:       return "PRINT";
        case TokenType::INPUT:       return "INPUT";
        case TokenType::AND:         return "AND";
        case TokenType::OR:          return "OR";
        case TokenType::NOT:         return "NOT";
        case TokenType::BREAK:       return "BREAK";
        case TokenType::CONTINUE:    return "CONTINUE";
        case TokenType::PLUS:        return "PLUS";
        case TokenType::MINUS:       return "MINUS";
        case TokenType::STAR:        return "STAR";
        case TokenType::SLASH:       return "SLASH";
        case TokenType::PERCENT:     return "PERCENT";
        case TokenType::EQ:          return "EQ";
        case TokenType::NEQ:         return "NEQ";
        case TokenType::LT:          return "LT";
        case TokenType::GT:          return "GT";
        case TokenType::LTE:         return "LTE";
        case TokenType::GTE:         return "GTE";
        case TokenType::ASSIGN:      return "ASSIGN";
        case TokenType::PLUS_ASSIGN: return "PLUS_ASSIGN";
        case TokenType::MINUS_ASSIGN:return "MINUS_ASSIGN";
        case TokenType::STAR_ASSIGN: return "STAR_ASSIGN";
        case TokenType::SLASH_ASSIGN:return "SLASH_ASSIGN";
        case TokenType::INCREMENT:   return "INCREMENT";
        case TokenType::DECREMENT:   return "DECREMENT";
        case TokenType::LPAREN:      return "LPAREN";
        case TokenType::RPAREN:      return "RPAREN";
        case TokenType::LBRACE:      return "LBRACE";
        case TokenType::RBRACE:      return "RBRACE";
        case TokenType::LBRACKET:    return "LBRACKET";
        case TokenType::RBRACKET:    return "RBRACKET";
        case TokenType::COMMA:       return "COMMA";
        case TokenType::SEMICOLON:   return "SEMICOLON";
        case TokenType::COLON:       return "COLON";
        case TokenType::DOT:         return "DOT";
        case TokenType::NEWLINE:     return "NEWLINE";
        case TokenType::EOF_TOKEN:   return "EOF";
        default:                     return "UNKNOWN";
    }
}

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int col;
    Token(TokenType t, std::string l, int ln, int c)
        : type(t), lexeme(std::move(l)), line(ln), col(c) {}
};

// ─────────────────────────────────────────────
//  Opcodes — the ISA
// ─────────────────────────────────────────────
enum class Opcode : uint8_t {
    // Stack ops
    PUSH_INT,       // push int64 constant (8 bytes follow)
    PUSH_FLOAT,     // push double constant (8 bytes follow)
    PUSH_BOOL,      // push bool (1 byte follows)
    PUSH_STRING,    // push string index (2 bytes follow)
    PUSH_NIL,       // push nil
    POP,            // discard top

    // Arithmetic
    ADD, SUB, MUL, DIV, MOD,
    NEG,            // unary minus

    // Comparison
    EQ, NEQ, LT, GT, LTE, GTE,

    // Logical
    AND, OR, NOT,

    // Variables
    LOAD_VAR,       // load var by index (2 bytes)
    STORE_VAR,      // store to var index (2 bytes)
    DEFINE_VAR,     // define new var at index (2 bytes)

    // Globals (for REPL)
    LOAD_GLOBAL,    // load global by name index (2 bytes)
    STORE_GLOBAL,   // store global by name index (2 bytes)

    // Control flow
    JUMP,           // unconditional jump (4 bytes offset)
    JUMP_IF_FALSE,  // conditional jump (4 bytes offset)
    JUMP_IF_TRUE,   // for OR short-circuit (4 bytes offset)

    // Functions
    CALL,           // call function (1 byte: arg count)
    RETURN,         // return from function
    RETURN_NIL,     // return nil (no explicit return)
    MAKE_FUNC,      // push function object (2 bytes: const idx)

    // I/O
    PRINT,          // pop and print
    INPUT,          // push string from stdin

    // String ops (bonus)
    STR_CONCAT,     // pop 2, concat, push

    // Introspection
    HALT,           // stop execution
    DEBUG_BREAK,    // debugger break
    NOP,            // no-op
};

inline std::string opcode_name(Opcode op) {
    switch(op) {
        case Opcode::PUSH_INT:    return "PUSH_INT";
        case Opcode::PUSH_FLOAT:  return "PUSH_FLOAT";
        case Opcode::PUSH_BOOL:   return "PUSH_BOOL";
        case Opcode::PUSH_STRING: return "PUSH_STRING";
        case Opcode::PUSH_NIL:    return "PUSH_NIL";
        case Opcode::POP:         return "POP";
        case Opcode::ADD:         return "ADD";
        case Opcode::SUB:         return "SUB";
        case Opcode::MUL:         return "MUL";
        case Opcode::DIV:         return "DIV";
        case Opcode::MOD:         return "MOD";
        case Opcode::NEG:         return "NEG";
        case Opcode::EQ:          return "EQ";
        case Opcode::NEQ:         return "NEQ";
        case Opcode::LT:          return "LT";
        case Opcode::GT:          return "GT";
        case Opcode::LTE:         return "LTE";
        case Opcode::GTE:         return "GTE";
        case Opcode::AND:         return "AND";
        case Opcode::OR:          return "OR";
        case Opcode::NOT:         return "NOT";
        case Opcode::LOAD_VAR:    return "LOAD_VAR";
        case Opcode::STORE_VAR:   return "STORE_VAR";
        case Opcode::DEFINE_VAR:  return "DEFINE_VAR";
        case Opcode::LOAD_GLOBAL: return "LOAD_GLOBAL";
        case Opcode::STORE_GLOBAL:return "STORE_GLOBAL";
        case Opcode::JUMP:        return "JUMP";
        case Opcode::JUMP_IF_FALSE:return "JUMP_IF_FALSE";
        case Opcode::JUMP_IF_TRUE:return "JUMP_IF_TRUE";
        case Opcode::CALL:        return "CALL";
        case Opcode::RETURN:      return "RETURN";
        case Opcode::RETURN_NIL:  return "RETURN_NIL";
        case Opcode::MAKE_FUNC:   return "MAKE_FUNC";
        case Opcode::PRINT:       return "PRINT";
        case Opcode::INPUT:       return "INPUT";
        case Opcode::STR_CONCAT:  return "STR_CONCAT";
        case Opcode::HALT:        return "HALT";
        case Opcode::DEBUG_BREAK: return "DEBUG_BREAK";
        case Opcode::NOP:         return "NOP";
        default:                  return "??";
    }
}

// ─────────────────────────────────────────────
//  Chunk — a compiled bytecode unit
// ─────────────────────────────────────────────
struct Chunk {
    std::vector<uint8_t>      code;
    std::vector<int64_t>      int_constants;
    std::vector<double>       float_constants;
    std::vector<std::string>  string_constants;
    std::vector<int>          lines;   // line info per instruction
    std::string               name;

    void write(uint8_t byte, int line) {
        code.push_back(byte);
        lines.push_back(line);
    }
    void write_opcode(Opcode op, int line) {
        write(static_cast<uint8_t>(op), line);
    }
    void write_uint16(uint16_t v, int line) {
        write((v >> 8) & 0xFF, line);
        write(v & 0xFF, line);
    }
    void write_uint32(uint32_t v, int line) {
        write((v >> 24) & 0xFF, line);
        write((v >> 16) & 0xFF, line);
        write((v >> 8)  & 0xFF, line);
        write(v & 0xFF, line);
    }
    void write_int64(int64_t v, int line) {
        uint64_t u = static_cast<uint64_t>(v);
        for (int i = 7; i >= 0; --i) write((u >> (i*8)) & 0xFF, line);
    }
    void write_double(double v, int line) {
        uint64_t u; memcpy(&u, &v, 8);
        for (int i = 7; i >= 0; --i) write((u >> (i*8)) & 0xFF, line);
    }

    uint16_t add_string(const std::string& s) {
        for (size_t i = 0; i < string_constants.size(); ++i)
            if (string_constants[i] == s) return (uint16_t)i;
        string_constants.push_back(s);
        return (uint16_t)(string_constants.size() - 1);
    }

    // Emit a placeholder jump and return its offset for patching
    size_t emit_jump(Opcode op, int line) {
        write_opcode(op, line);
        write(0xFF, line); write(0xFF, line); write(0xFF, line); write(0xFF, line);
        return code.size() - 4;
    }
    void patch_jump(size_t offset, size_t target) {
        uint32_t v = (uint32_t)target;
        code[offset]     = (v >> 24) & 0xFF;
        code[offset + 1] = (v >> 16) & 0xFF;
        code[offset + 2] = (v >> 8)  & 0xFF;
        code[offset + 3] =  v & 0xFF;
    }
};

// ─────────────────────────────────────────────
//  Error types
// ─────────────────────────────────────────────
struct CVMError : std::runtime_error {
    int line; int col;
    CVMError(const std::string& msg, int l = -1, int c = -1)
        : std::runtime_error(msg), line(l), col(c) {}
};

struct LexError   : CVMError { using CVMError::CVMError; };
struct ParseError : CVMError { using CVMError::CVMError; };
struct CompileError: CVMError{ using CVMError::CVMError; };
struct RuntimeError: CVMError{ using CVMError::CVMError; };

} // namespace cvm
