#include "vm.hpp"
#include <stdexcept>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <sstream>

namespace cvm {

VM::VM() { register_stdlib(); }

void VM::register_native(const std::string& name, NativeFn fn) {
    natives_[name] = std::move(fn);
}

// ─── Stack ops ──────────────────────────────
void VM::push(Value v) {
    stack_.push_back(std::move(v));
    if (stack_.size() > stats_.stack_peak)
        stats_.stack_peak = stack_.size();
}

Value VM::pop() {
    if (stack_.empty()) throw RuntimeError("Stack underflow");
    Value v = std::move(stack_.back());
    stack_.pop_back();
    return v;
}

Value& VM::top() {
    if (stack_.empty()) throw RuntimeError("Stack underflow on top()");
    return stack_.back();
}

Value VM::peek_at(size_t offset) {
    if (offset >= stack_.size()) throw RuntimeError("Stack underflow on peek");
    return stack_[stack_.size() - 1 - offset];
}

// ─── Byte reading ────────────────────────────
uint8_t VM::read_byte() {
    auto& f = current_frame();
    return f.chunk->code[f.ip++];
}
uint16_t VM::read_uint16() {
    uint16_t hi = read_byte(), lo = read_byte();
    return (hi << 8) | lo;
}
uint32_t VM::read_uint32() {
    uint32_t a = read_byte(), b = read_byte(), c = read_byte(), d = read_byte();
    return (a << 24) | (b << 16) | (c << 8) | d;
}
int64_t VM::read_int64() {
    int64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | (uint8_t)read_byte();
    return v;
}
double VM::read_double() {
    uint64_t u = 0;
    for (int i = 0; i < 8; ++i) u = (u << 8) | (uint8_t)read_byte();
    double d; memcpy(&d, &u, 8); return d;
}

// ─── Arithmetic ──────────────────────────────
Value VM::do_add(const Value& a, const Value& b) {
    if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b))
        return std::get<int64_t>(a) + std::get<int64_t>(b);
    if (std::holds_alternative<double>(a)  && std::holds_alternative<double>(b))
        return std::get<double>(a) + std::get<double>(b);
    if (std::holds_alternative<int64_t>(a) && std::holds_alternative<double>(b))
        return (double)std::get<int64_t>(a) + std::get<double>(b);
    if (std::holds_alternative<double>(a)  && std::holds_alternative<int64_t>(b))
        return std::get<double>(a) + (double)std::get<int64_t>(b);
    if (std::holds_alternative<std::string>(a) || std::holds_alternative<std::string>(b))
        return value_to_string(a) + value_to_string(b);
    throw RuntimeError("Cannot add values of incompatible types");
}
Value VM::do_sub(const Value& a, const Value& b) {
    if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b))
        return std::get<int64_t>(a) - std::get<int64_t>(b);
    if (std::holds_alternative<double>(a)  && std::holds_alternative<double>(b))
        return std::get<double>(a) - std::get<double>(b);
    if (std::holds_alternative<int64_t>(a) && std::holds_alternative<double>(b))
        return (double)std::get<int64_t>(a) - std::get<double>(b);
    if (std::holds_alternative<double>(a)  && std::holds_alternative<int64_t>(b))
        return std::get<double>(a) - (double)std::get<int64_t>(b);
    throw RuntimeError("Cannot subtract non-numeric types");
}
Value VM::do_mul(const Value& a, const Value& b) {
    if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b))
        return std::get<int64_t>(a) * std::get<int64_t>(b);
    if (std::holds_alternative<double>(a)  && std::holds_alternative<double>(b))
        return std::get<double>(a) * std::get<double>(b);
    if (std::holds_alternative<int64_t>(a) && std::holds_alternative<double>(b))
        return (double)std::get<int64_t>(a) * std::get<double>(b);
    if (std::holds_alternative<double>(a)  && std::holds_alternative<int64_t>(b))
        return std::get<double>(a) * (double)std::get<int64_t>(b);
    if (std::holds_alternative<std::string>(a) && std::holds_alternative<int64_t>(b)) {
        std::string r; int64_t n = std::get<int64_t>(b);
        for (int64_t i = 0; i < n; ++i) r += std::get<std::string>(a);
        return r;
    }
    throw RuntimeError("Cannot multiply incompatible types");
}
Value VM::do_div(const Value& a, const Value& b) {
    auto to_d = [](const Value& v) -> double {
        if (std::holds_alternative<int64_t>(v)) return (double)std::get<int64_t>(v);
        return std::get<double>(v);
    };
    if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
        if (std::get<int64_t>(b) == 0) throw RuntimeError("Division by zero");
        return std::get<int64_t>(a) / std::get<int64_t>(b);
    }
    double dv = to_d(b);
    if (dv == 0.0) throw RuntimeError("Division by zero");
    return to_d(a) / dv;
}
Value VM::do_mod(const Value& a, const Value& b) {
    if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
        if (std::get<int64_t>(b) == 0) throw RuntimeError("Modulo by zero");
        return std::get<int64_t>(a) % std::get<int64_t>(b);
    }
    throw RuntimeError("Modulo requires integer operands");
}
Value VM::do_neg(const Value& a) {
    if (std::holds_alternative<int64_t>(a)) return -std::get<int64_t>(a);
    if (std::holds_alternative<double>(a))  return -std::get<double>(a);
    throw RuntimeError("Cannot negate non-numeric value");
}

// ─────────────────────────────────────────────
//  CALL CONVENTION
// ─────────────────────────────────────────────
// Stack just before CALL executes (argc=2 example):
//   [ ... | fn_ref | arg0 | arg1 ]
//                    ^base        ^top
//  base = stack_.size() - argc
//  fn_ref is at stack_[base - 1]
//
// After CALL sets up frame:
//   frame.base = base - 1   (fn_ref slot becomes param slot 0's predecessor)
//   Actually we keep args exactly where they are and set base = index of arg0
//   frame.base = base  (arg0 is at stack_[frame.base + 0])
//
// DEFINE_VAR slot=i pops from stack top and writes to stack_[frame.base + i].
// Since args were pushed left-to-right (arg0 first, arg1 last),
// and DEFINE_VAR processes them in order 0,1,...
// we must NOT pop — args are already laid out in order at base..base+argc-1.
//
// NEW APPROACH: Remove the DEFINE_VAR prolog from function compilation entirely.
// The CALL handler simply sets frame.base to the position of arg0 on the stack.
// LOAD_VAR slot=i  →  stack_[frame.base + i]  (directly reads placed args)
// STORE_VAR slot=i →  stack_[frame.base + i]
// Local variables defined with LET inside the function get slots argc, argc+1, ...
// which are above the args — we grow the stack as needed.
//
// RETURN: pop the return value, shrink stack back to frame.base - 1
//         (removing the fn_ref too), push return value.
// ─────────────────────────────────────────────

VMResult VM::execute(const Compiler::Result& compiled) {
    compiled_ = &compiled;
    frames_.clear();
    stack_.clear();

    frames_.push_back({&compiled.main_chunk, 0, 0, "main"});
    return run();
}

VMResult VM::run() {
    using Op = Opcode;

    while (true) {
        if (frames_.empty()) break;
        auto& frame = current_frame();
        if (frame.ip >= frame.chunk->code.size()) break;

        ++stats_.instructions_executed;
        uint8_t raw = read_byte();
        Op op = static_cast<Op>(raw);

        switch (op) {

        // ─── Push constants ─────────────────────────────────────────
        case Op::PUSH_INT:    push(read_int64()); break;
        case Op::PUSH_FLOAT:  push(read_double()); break;
        case Op::PUSH_BOOL:   push((bool)read_byte()); break;
        case Op::PUSH_STRING: {
            uint16_t idx = read_uint16();
            push(frame.chunk->string_constants[idx]);
            break;
        }
        case Op::PUSH_NIL:    push(std::monostate{}); break;
        case Op::POP:         pop(); break;

        // ─── Arithmetic ─────────────────────────────────────────────
        case Op::ADD: { auto b = pop(), a = pop(); push(do_add(a, b)); break; }
        case Op::SUB: { auto b = pop(), a = pop(); push(do_sub(a, b)); break; }
        case Op::MUL: { auto b = pop(), a = pop(); push(do_mul(a, b)); break; }
        case Op::DIV: { auto b = pop(), a = pop(); push(do_div(a, b)); break; }
        case Op::MOD: { auto b = pop(), a = pop(); push(do_mod(a, b)); break; }
        case Op::NEG: { auto a = pop(); push(do_neg(a)); break; }

        // ─── Comparison ─────────────────────────────────────────────
        case Op::EQ:  { auto b = pop(), a = pop(); push(value_equal(a, b)); break; }
        case Op::NEQ: { auto b = pop(), a = pop(); push(!value_equal(a, b)); break; }
        case Op::LT:  { auto b = pop(), a = pop(); push(value_less(a, b)); break; }
        case Op::GT:  { auto b = pop(), a = pop(); push(value_less(b, a)); break; }
        case Op::LTE: { auto b = pop(), a = pop(); push(!value_less(b, a)); break; }
        case Op::GTE: { auto b = pop(), a = pop(); push(!value_less(a, b)); break; }

        // ─── Logical ────────────────────────────────────────────────
        case Op::AND: { auto b = pop(), a = pop(); push(value_truthy(a) && value_truthy(b)); break; }
        case Op::OR:  { auto b = pop(), a = pop(); push(value_truthy(a) || value_truthy(b)); break; }
        case Op::NOT: { auto a = pop(); push(!value_truthy(a)); break; }

        // ─── Local variables ────────────────────────────────────────
        case Op::DEFINE_VAR: {
            // Used only for LET statements inside function/block bodies.
            // NOT used for function parameters anymore.
            uint16_t slot = read_uint16();
            Value v = pop();
            size_t abs = frame.base + slot;
            // Grow the stack to accommodate the slot
            while (stack_.size() <= abs) stack_.push_back(std::monostate{});
            stack_[abs] = std::move(v);
            break;
        }
        case Op::LOAD_VAR: {
            uint16_t slot = read_uint16();
            size_t abs = frame.base + slot;
            if (abs >= stack_.size()) push(std::monostate{});
            else push(stack_[abs]);
            break;
        }
        case Op::STORE_VAR: {
            uint16_t slot = read_uint16();
            size_t abs = frame.base + slot;
            while (stack_.size() <= abs) stack_.push_back(std::monostate{});
            // assignment is an expression; leave value on stack
            stack_[abs] = top();
            break;
        }

        // ─── Global variables ────────────────────────────────────────
        case Op::LOAD_GLOBAL: {
            uint16_t idx = read_uint16();
            const std::string& name = frame.chunk->string_constants[idx];
            auto it = globals_.find(name);
            if (it != globals_.end()) { push(it->second); break; }
            auto nit = natives_.find(name);
            if (nit != natives_.end()) { push(std::string("__native__:" + name)); break; }
            push(std::monostate{});
            break;
        }
        case Op::STORE_GLOBAL: {
            uint16_t idx = read_uint16();
            const std::string& name = frame.chunk->string_constants[idx];
            globals_[name] = top(); // assignment is expression, don't pop
            break;
        }

        // ─── Jumps ──────────────────────────────────────────────────
        case Op::JUMP: {
            uint32_t target = read_uint32();
            frame.ip = target;
            break;
        }
        case Op::JUMP_IF_FALSE: {
            uint32_t target = read_uint32();
            if (!value_truthy(top())) frame.ip = target;
            break;
        }
        case Op::JUMP_IF_TRUE: {
            uint32_t target = read_uint32();
            if (value_truthy(top())) frame.ip = target;
            break;
        }

        // ─── Functions ──────────────────────────────────────────────
        case Op::MAKE_FUNC: {
            uint16_t fn_idx = read_uint16();
            push(std::string("__fn__:" + std::to_string(fn_idx)));
            break;
        }

        case Op::CALL: {
            uint8_t argc = read_byte();
            ++stats_.function_calls;

            // Stack layout: [ ... fn_ref arg0 arg1 ... argN-1 ]
            //                     ^-- fn_pos    ^-- base (= fn_pos + 1)
            size_t fn_pos = stack_.size() - argc - 1;
            Value fn_ref  = stack_[fn_pos];

            if (!std::holds_alternative<std::string>(fn_ref))
                throw RuntimeError("Cannot call non-function value: " + value_to_string(fn_ref));

            const std::string& fn_str = std::get<std::string>(fn_ref);

            // ── Native function ──
            if (fn_str.rfind("__native__:", 0) == 0) {
                std::string native_name = fn_str.substr(11);
                auto it = natives_.find(native_name);
                if (it == natives_.end())
                    throw RuntimeError("Unknown native: " + native_name);

                // Collect args (they're on stack above fn_ref, in order)
                std::vector<Value> args;
                args.reserve(argc);
                for (size_t i = 0; i < argc; ++i)
                    args.push_back(stack_[fn_pos + 1 + i]);

                // Remove fn_ref + args from stack
                stack_.resize(fn_pos);

                Value result = it->second(args);
                push(result);
                break;
            }

            // ── User function ──
            if (fn_str.rfind("__fn__:", 0) != 0)
                throw RuntimeError("Cannot call: " + fn_str);

            int fn_idx = std::stoi(fn_str.substr(7));
            if (fn_idx < 0 || fn_idx >= (int)compiled_->functions.size())
                throw RuntimeError("Invalid function index");

            const FunctionProto& proto = compiled_->functions[fn_idx];
            if (argc != (uint8_t)proto.arity)
                throw RuntimeError("'" + proto.name + "' expects " +
                    std::to_string(proto.arity) + " args, got " + std::to_string(argc));

            // Remove the fn_ref slot by overwriting it with arg0,
            // then the args naturally occupy [fn_pos .. fn_pos+argc-1].
            // We set frame.base = fn_pos so that param slot 0 = stack_[fn_pos+0].
            for (size_t i = 0; i < argc; ++i)
                stack_[fn_pos + i] = std::move(stack_[fn_pos + 1 + i]);
            stack_.resize(fn_pos + argc);

            // Push new call frame; base points to arg0's stack position
            frames_.push_back({&proto.chunk, 0, fn_pos, proto.name});
            break;
        }

        case Op::RETURN: {
            Value retval = pop();
            size_t base = frame.base;
            frames_.pop_back();
            // Remove everything this call placed on the stack (args + locals)
            stack_.resize(base);
            push(retval);
            break;
        }
        case Op::RETURN_NIL: {
            size_t base = frame.base;
            frames_.pop_back();
            stack_.resize(base);
            push(std::monostate{});
            break;
        }

        // ─── I/O ────────────────────────────────────────────────────
        case Op::PRINT: {
            Value v = pop();
            *out_ << value_to_string(v) << "\n";
            break;
        }
        case Op::INPUT: {
            Value prompt = pop();
            std::string p = value_to_string(prompt);
            if (!p.empty()) *out_ << p << std::flush;
            std::string line;
            std::getline(*in_, line);
            // Try int → float → string
            try { push(int64_t(std::stoll(line))); break; } catch(...) {}
            try { push(std::stod(line)); break; }           catch(...) {}
            push(line);
            break;
        }
        case Op::STR_CONCAT: {
            auto b = pop(), a = pop();
            push(value_to_string(a) + value_to_string(b));
            break;
        }

        case Op::NOP: break;
        case Op::DEBUG_BREAK:
            *out_ << "[DEBUG at " << frame.fn_name << " ip=" << (frame.ip-1) << "]\n";
            break;
        case Op::HALT:
            goto done;

        default:
            throw RuntimeError("Unknown opcode: " + std::to_string(raw));
        }
    }

done:
    VMResult res;
    res.ok = true;
    res.retval = stack_.empty() ? Value(std::monostate{}) : top();
    return res;
}

// ─── Standard Library ─────────────────────────
void VM::register_stdlib() {
    register_native("sqrt", [](std::vector<Value> args) -> Value {
        if (args.empty()) throw RuntimeError("sqrt() requires 1 argument");
        double v = std::holds_alternative<int64_t>(args[0]) ?
            (double)std::get<int64_t>(args[0]) : std::get<double>(args[0]);
        return std::sqrt(v);
    });
    register_native("abs", [](std::vector<Value> args) -> Value {
        if (args.empty()) throw RuntimeError("abs() requires 1 argument");
        if (std::holds_alternative<int64_t>(args[0])) return std::abs(std::get<int64_t>(args[0]));
        return std::abs(std::get<double>(args[0]));
    });
    register_native("pow", [](std::vector<Value> args) -> Value {
        if (args.size() < 2) throw RuntimeError("pow() requires 2 arguments");
        auto to_d = [](const Value& v) {
            return std::holds_alternative<int64_t>(v) ?
                (double)std::get<int64_t>(v) : std::get<double>(v);
        };
        return std::pow(to_d(args[0]), to_d(args[1]));
    });
    register_native("floor", [](std::vector<Value> args) -> Value {
        if (args.empty()) throw RuntimeError("floor() requires 1 argument");
        double v = std::holds_alternative<int64_t>(args[0]) ?
            (double)std::get<int64_t>(args[0]) : std::get<double>(args[0]);
        return std::floor(v);
    });
    register_native("ceil", [](std::vector<Value> args) -> Value {
        if (args.empty()) throw RuntimeError("ceil() requires 1 argument");
        double v = std::holds_alternative<int64_t>(args[0]) ?
            (double)std::get<int64_t>(args[0]) : std::get<double>(args[0]);
        return std::ceil(v);
    });
    register_native("round", [](std::vector<Value> args) -> Value {
        if (args.empty()) throw RuntimeError("round() requires 1 argument");
        double v = std::holds_alternative<int64_t>(args[0]) ?
            (double)std::get<int64_t>(args[0]) : std::get<double>(args[0]);
        return std::round(v);
    });
    register_native("max", [](std::vector<Value> args) -> Value {
        if (args.size() < 2) throw RuntimeError("max() requires 2 arguments");
        return value_less(args[0], args[1]) ? args[1] : args[0];
    });
    register_native("min", [](std::vector<Value> args) -> Value {
        if (args.size() < 2) throw RuntimeError("min() requires 2 arguments");
        return value_less(args[0], args[1]) ? args[0] : args[1];
    });
    register_native("str", [](std::vector<Value> args) -> Value {
        if (args.empty()) return std::string("");
        return value_to_string(args[0]);
    });
    register_native("int", [](std::vector<Value> args) -> Value {
        if (args.empty()) return int64_t(0);
        if (std::holds_alternative<std::string>(args[0]))
            return int64_t(std::stoll(std::get<std::string>(args[0])));
        if (std::holds_alternative<double>(args[0]))
            return int64_t(std::get<double>(args[0]));
        if (std::holds_alternative<bool>(args[0]))
            return int64_t(std::get<bool>(args[0]) ? 1 : 0);
        return args[0];
    });
    register_native("float", [](std::vector<Value> args) -> Value {
        if (args.empty()) return 0.0;
        if (std::holds_alternative<std::string>(args[0]))
            return std::stod(std::get<std::string>(args[0]));
        if (std::holds_alternative<int64_t>(args[0]))
            return (double)std::get<int64_t>(args[0]);
        if (std::holds_alternative<bool>(args[0]))
            return std::get<bool>(args[0]) ? 1.0 : 0.0;
        return args[0];
    });
    register_native("len", [](std::vector<Value> args) -> Value {
        if (args.empty()) throw RuntimeError("len() requires 1 argument");
        if (std::holds_alternative<std::string>(args[0]))
            return int64_t(std::get<std::string>(args[0]).size());
        throw RuntimeError("len() requires a string argument");
    });
    register_native("type", [](std::vector<Value> args) -> Value {
        if (args.empty()) return std::string("nil");
        return std::visit([](auto&& v) -> Value {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) return std::string("nil");
            else if constexpr (std::is_same_v<T, int64_t>)   return std::string("int");
            else if constexpr (std::is_same_v<T, double>)    return std::string("float");
            else if constexpr (std::is_same_v<T, bool>)      return std::string("bool");
            else if constexpr (std::is_same_v<T, std::string>) return std::string("string");
        }, args[0]);
    });
    register_native("bool", [](std::vector<Value> args) -> Value {
        if (args.empty()) return false;
        return value_truthy(args[0]);
    });
    register_native("chr", [](std::vector<Value> args) -> Value {
        if (args.empty()) throw RuntimeError("chr() requires 1 argument");
        int64_t code = std::get<int64_t>(args[0]);
        return std::string(1, (char)code);
    });
    register_native("ord", [](std::vector<Value> args) -> Value {
        if (args.empty()) throw RuntimeError("ord() requires 1 argument");
        const std::string& s = std::get<std::string>(args[0]);
        if (s.empty()) throw RuntimeError("ord() requires non-empty string");
        return int64_t((unsigned char)s[0]);
    });
}

} // namespace cvm
