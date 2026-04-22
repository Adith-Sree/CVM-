#pragma once
#include "cvm_types.hpp"
#include "compiler.hpp"
#include <vector>
#include <unordered_map>
#include <functional>
#include <string>
#include <iostream>

namespace cvm {

// ─────────────────────────────────────────────
//  Call Frame (one per active function call)
// ─────────────────────────────────────────────
struct CallFrame {
    const Chunk* chunk = nullptr;
    size_t       ip    = 0;       // instruction pointer
    size_t       base  = 0;       // base stack index for locals
    std::string  fn_name = "main";
};

// ─────────────────────────────────────────────
//  VM Execution Result
// ─────────────────────────────────────────────
struct VMResult {
    bool   ok      = true;
    Value  retval  = std::monostate{};
    std::string error;
};

// ─────────────────────────────────────────────
//  Native function type
// ─────────────────────────────────────────────
using NativeFn = std::function<Value(std::vector<Value>)>;

// ─────────────────────────────────────────────
//  The Virtual Machine
// ─────────────────────────────────────────────
class VM {
public:
    VM();

    // Configure I/O streams (for testing/embedding)
    void set_output(std::ostream* out) { out_ = out; }
    void set_input(std::istream* in)   { in_  = in;  }

    // Register a native (C++) function
    void register_native(const std::string& name, NativeFn fn);

    // Execute compiled result
    VMResult execute(const Compiler::Result& compiled);

    // Stack inspection (for REPL / debug)
    const std::vector<Value>& stack() const { return stack_; }
    const std::unordered_map<std::string, Value>& globals() const { return globals_; }
    void reset_globals() { globals_.clear(); }

    // Execution stats
    struct Stats {
        uint64_t instructions_executed = 0;
        uint64_t stack_peak            = 0;
        uint64_t function_calls        = 0;
    };
    const Stats& stats() const { return stats_; }
    void reset_stats()         { stats_ = {}; }

private:
    std::vector<Value>  stack_;
    std::vector<CallFrame> frames_;
    std::unordered_map<std::string, Value> globals_;
    std::unordered_map<std::string, NativeFn> natives_;
    const Compiler::Result* compiled_ = nullptr;

    std::ostream* out_ = &std::cout;
    std::istream* in_  = &std::cin;

    Stats stats_;

    // Stack ops
    void push(Value v);
    Value pop();
    Value& top();
    Value peek_at(size_t offset = 0);

    // Byte reading helpers
    uint8_t  read_byte();
    uint16_t read_uint16();
    uint32_t read_uint32();
    int64_t  read_int64();
    double   read_double();

    // Execution loop
    VMResult run();

    // Arithmetic helpers
    Value do_add(const Value& a, const Value& b);
    Value do_sub(const Value& a, const Value& b);
    Value do_mul(const Value& a, const Value& b);
    Value do_div(const Value& a, const Value& b);
    Value do_mod(const Value& a, const Value& b);
    Value do_neg(const Value& a);

    // Register stdlib natives
    void register_stdlib();

    CallFrame& current_frame() { return frames_.back(); }
};

} // namespace cvm
