#include "compiler.hpp"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <stdexcept>

namespace cvm {

// ─── Helpers ────────────────────────────────
void Compiler::emit(Opcode op) {
    current_chunk_->write_opcode(op, current_line_);
}
void Compiler::emit_int(int64_t v) {
    emit(Opcode::PUSH_INT);
    current_chunk_->write_int64(v, current_line_);
}
void Compiler::emit_float(double v) {
    emit(Opcode::PUSH_FLOAT);
    current_chunk_->write_double(v, current_line_);
}
void Compiler::emit_bool(bool v) {
    emit(Opcode::PUSH_BOOL);
    current_chunk_->write(v ? 1 : 0, current_line_);
}
void Compiler::emit_string(const std::string& s) {
    emit(Opcode::PUSH_STRING);
    uint16_t idx = current_chunk_->add_string(s);
    current_chunk_->write_uint16(idx, current_line_);
}

void Compiler::push_scope(bool is_loop, bool is_fn) {
    Scope sc;
    sc.is_loop = is_loop;
    sc.is_function = is_fn;
    if (is_fn || scopes_.empty()) sc.next_slot = 0;
    else sc.next_slot = scopes_.back().next_slot + (uint16_t)scopes_.back().locals.size();
    scopes_.push_back(std::move(sc));
}

void Compiler::pop_scope() {
    if (!scopes_.empty()) scopes_.pop_back();
}

uint16_t Compiler::define_local(const std::string& name) {
    auto& sc = scopes_.back();
    uint16_t slot = sc.next_slot++;
    sc.locals[name] = slot;
    return slot;
}

std::optional<uint16_t> Compiler::resolve_local(const std::string& name) {
    for (int i = (int)scopes_.size() - 1; i >= 0; --i) {
        auto it = scopes_[i].locals.find(name);
        if (it != scopes_[i].locals.end()) return it->second;
        if (scopes_[i].is_function) break;
    }
    return std::nullopt;
}

void Compiler::emit_load(const std::string& name, int line) {
    current_line_ = line;
    auto slot = resolve_local(name);
    if (slot) {
        emit(Opcode::LOAD_VAR);
        current_chunk_->write_uint16(*slot, line);
    } else {
        emit(Opcode::LOAD_GLOBAL);
        uint16_t idx = current_chunk_->add_string(name);
        current_chunk_->write_uint16(idx, line);
    }
}

void Compiler::emit_store(const std::string& name, int line) {
    current_line_ = line;
    auto slot = resolve_local(name);
    if (slot) {
        emit(Opcode::STORE_VAR);
        current_chunk_->write_uint16(*slot, line);
    } else {
        emit(Opcode::STORE_GLOBAL);
        uint16_t idx = current_chunk_->add_string(name);
        current_chunk_->write_uint16(idx, line);
    }
}

// ─── Entry ──────────────────────────────────
Compiler::Result Compiler::compile(ASTNode& ast) {
    result_ = Result{};
    result_.main_chunk.name = "main";
    current_chunk_ = &result_.main_chunk;
    ast.accept(*this);
    emit(Opcode::HALT);
    return std::move(result_);
}

// ─── Visitor Implementations ─────────────────

void Compiler::visit(ProgramNode& n) {
    for (auto& s : n.stmts) {
        current_line_ = s->line;
        s->accept(*this);
    }
}

void Compiler::visit(LiteralExpr& n) {
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, int64_t>)         emit_int(v);
        else if constexpr (std::is_same_v<T, double>)     emit_float(v);
        else if constexpr (std::is_same_v<T, bool>)       emit_bool(v);
        else if constexpr (std::is_same_v<T, std::string>) emit_string(v);
        else emit(Opcode::PUSH_NIL);
    }, n.value);
}

void Compiler::visit(IdentifierExpr& n) {
    emit_load(n.name, n.line);
}

void Compiler::visit(BinaryExpr& n) {
    current_line_ = n.line;

    // Short-circuit AND
    if (n.op == "and") {
        n.left->accept(*this);
        size_t jmp = current_chunk_->emit_jump(Opcode::JUMP_IF_FALSE, n.line);
        emit(Opcode::POP);
        n.right->accept(*this);
        current_chunk_->patch_jump(jmp, current_chunk_->code.size());
        return;
    }
    // Short-circuit OR
    if (n.op == "or") {
        n.left->accept(*this);
        size_t jmp = current_chunk_->emit_jump(Opcode::JUMP_IF_TRUE, n.line);
        emit(Opcode::POP);
        n.right->accept(*this);
        current_chunk_->patch_jump(jmp, current_chunk_->code.size());
        return;
    }

    n.left->accept(*this);
    n.right->accept(*this);

    static const std::unordered_map<std::string, Opcode> bin_ops = {
        {"+",  Opcode::ADD}, {"-", Opcode::SUB}, {"*", Opcode::MUL},
        {"/",  Opcode::DIV}, {"%", Opcode::MOD},
        {"==", Opcode::EQ},  {"!=",Opcode::NEQ},
        {"<",  Opcode::LT},  {">", Opcode::GT},
        {"<=", Opcode::LTE}, {">=",Opcode::GTE},
    };
    auto it = bin_ops.find(n.op);
    if (it != bin_ops.end()) emit(it->second);
    else throw CompileError("Unknown binary op: " + n.op, n.line);
}

void Compiler::visit(UnaryExpr& n) {
    n.operand->accept(*this);
    if (n.op == "-")        emit(Opcode::NEG);
    else if (n.op == "not") emit(Opcode::NOT);
    else throw CompileError("Unknown unary op: " + n.op, n.line);
}

void Compiler::visit(AssignExpr& n) {
    current_line_ = n.line;
    n.value->accept(*this);
    emit_store(n.name, n.line);
}

void Compiler::visit(CompoundAssignExpr& n) {
    current_line_ = n.line;
    emit_load(n.name, n.line);
    n.value->accept(*this);
    static const std::unordered_map<std::string, Opcode> ops = {
        {"+=", Opcode::ADD}, {"-=", Opcode::SUB},
        {"*=", Opcode::MUL}, {"/=", Opcode::DIV},
    };
    auto it = ops.find(n.op);
    if (it != ops.end()) emit(it->second);
    else throw CompileError("Unknown compound op: " + n.op, n.line);
    emit_store(n.name, n.line);
}

void Compiler::visit(CallExpr& n) {
    current_line_ = n.line;
    emit_load(n.callee, n.line);
    for (auto& arg : n.args) arg->accept(*this);
    emit(Opcode::CALL);
    current_chunk_->write((uint8_t)n.args.size(), n.line);
}

void Compiler::visit(InputExpr& n) {
    if (n.prompt) n.prompt->accept(*this);
    else emit_string("");
    emit(Opcode::INPUT);
}

void Compiler::visit(ExprStmt& n) {
    current_line_ = n.line;
    n.expr->accept(*this);
    emit(Opcode::POP);
}

void Compiler::visit(LetStmt& n) {
    current_line_ = n.line;
    if (n.initializer) n.initializer->accept(*this);
    else emit(Opcode::PUSH_NIL);

    if (!scopes_.empty()) {
        uint16_t slot = define_local(n.name);
        emit(Opcode::DEFINE_VAR);
        current_chunk_->write_uint16(slot, n.line);
    } else {
        emit(Opcode::STORE_GLOBAL);
        uint16_t idx = current_chunk_->add_string(n.name);
        current_chunk_->write_uint16(idx, n.line);
        emit(Opcode::POP);
    }
}

void Compiler::visit(PrintStmt& n) {
    current_line_ = n.line;
    for (size_t i = 0; i < n.exprs.size(); ++i) {
        n.exprs[i]->accept(*this);
        emit(Opcode::PRINT);
    }
    if (n.exprs.empty()) {
        emit_string("");
        emit(Opcode::PRINT);
    }
}

void Compiler::visit(BlockStmt& n) {
    push_scope();
    for (auto& s : n.stmts) {
        current_line_ = s->line;
        s->accept(*this);
    }
    pop_scope();
}

void Compiler::visit(IfStmt& n) {
    current_line_ = n.line;
    n.condition->accept(*this);
    // JUMP_IF_FALSE peeks (does NOT pop) the condition.
    // We emit POP on BOTH the taken and not-taken paths.
    size_t else_jmp = current_chunk_->emit_jump(Opcode::JUMP_IF_FALSE, n.line);
    // --- true path ---
    emit(Opcode::POP);            // pop condition (true)
    n.then_branch->accept(*this);

    if (n.else_branch) {
        size_t end_jmp = current_chunk_->emit_jump(Opcode::JUMP, n.line);
        // --- false path ---
        current_chunk_->patch_jump(else_jmp, current_chunk_->code.size());
        emit(Opcode::POP);        // pop condition (false)
        n.else_branch->accept(*this);
        current_chunk_->patch_jump(end_jmp, current_chunk_->code.size());
    } else {
        // --- false path (no else): skip body, pop condition ---
        // We need a jump over the pop that was already done in the true path.
        // Insert a JUMP past a POP, then patch else_jmp to the POP.
        size_t skip_jmp = current_chunk_->emit_jump(Opcode::JUMP, n.line);
        current_chunk_->patch_jump(else_jmp, current_chunk_->code.size());
        emit(Opcode::POP);        // pop condition (false, no body)
        current_chunk_->patch_jump(skip_jmp, current_chunk_->code.size());
    }
}

void Compiler::visit(WhileStmt& n) {
    current_line_ = n.line;

    // Push a loop context. continue_patch_target = SIZE_MAX means
    // we patch continues directly to loop_start after body compile.
    loop_stack_.push_back({});
    LoopContext& ctx = loop_stack_.back();
    ctx.loop_start = current_chunk_->code.size();

    n.condition->accept(*this);
    size_t exit_jmp = current_chunk_->emit_jump(Opcode::JUMP_IF_FALSE, n.line);
    emit(Opcode::POP);

    n.body->accept(*this);

    // Jump back to condition
    emit(Opcode::JUMP);
    current_chunk_->write_uint32((uint32_t)ctx.loop_start, n.line);

    size_t after_loop = current_chunk_->code.size();
    current_chunk_->patch_jump(exit_jmp, after_loop);
    emit(Opcode::POP);

    // Patch all break jumps → after loop
    for (auto off : ctx.break_offsets)
        current_chunk_->patch_jump(off, current_chunk_->code.size());

    // Patch all continue jumps → loop condition (loop_start)
    for (auto off : ctx.continue_targets)
        current_chunk_->patch_jump(off, ctx.loop_start);

    loop_stack_.pop_back();
}

void Compiler::visit(ForStmt& n) {
    current_line_ = n.line;
    push_scope(true);
    if (n.init) n.init->accept(*this);

    loop_stack_.push_back({});
    LoopContext& ctx = loop_stack_.back();
    ctx.loop_start = current_chunk_->code.size();

    size_t exit_jmp = SIZE_MAX;
    if (n.condition) {
        n.condition->accept(*this);
        exit_jmp = current_chunk_->emit_jump(Opcode::JUMP_IF_FALSE, n.line);
        emit(Opcode::POP);
    }

    n.body->accept(*this);

    // Continue → increment then back to condition
    size_t cont_target = current_chunk_->code.size();
    if (n.increment) {
        n.increment->accept(*this);
        emit(Opcode::POP);
    }
    emit(Opcode::JUMP);
    current_chunk_->write_uint32((uint32_t)ctx.loop_start, n.line);

    if (exit_jmp != SIZE_MAX) {
        current_chunk_->patch_jump(exit_jmp, current_chunk_->code.size());
        emit(Opcode::POP);
    }

    // Patch break → after loop
    for (auto off : ctx.break_offsets)
        current_chunk_->patch_jump(off, current_chunk_->code.size());

    // Patch continue → increment section
    for (auto off : ctx.continue_targets)
        current_chunk_->patch_jump(off, cont_target);

    loop_stack_.pop_back();
    pop_scope();
}

void Compiler::visit(FnStmt& n) {
    current_line_ = n.line;

    FunctionProto proto;
    proto.name   = n.name;
    proto.arity  = (int)n.params.size();
    proto.params = n.params;
    proto.chunk.name = n.name;

    Chunk* saved_chunk = current_chunk_;
    current_chunk_ = &proto.chunk;
    bool saved_in_fn = in_function_;
    in_function_ = true;

    push_scope(false, true);
    for (auto& p : n.params) define_local(p);


    n.body->accept(*this);
    emit(Opcode::RETURN_NIL);
    pop_scope();

    in_function_ = saved_in_fn;
    current_chunk_ = saved_chunk;

    int fn_idx = (int)result_.functions.size();
    result_.fn_name_to_idx[n.name] = fn_idx;
    result_.functions.push_back(std::move(proto));

    emit(Opcode::MAKE_FUNC);
    current_chunk_->write_uint16((uint16_t)fn_idx, n.line);
    emit(Opcode::STORE_GLOBAL);
    uint16_t name_idx = current_chunk_->add_string(n.name);
    current_chunk_->write_uint16(name_idx, n.line);
}

void Compiler::visit(ReturnStmt& n) {
    current_line_ = n.line;
    if (n.value) n.value->accept(*this);
    else emit(Opcode::PUSH_NIL);
    emit(Opcode::RETURN);
}

void Compiler::visit(BreakStmt&) {
    if (loop_stack_.empty()) throw CompileError("'break' outside loop", current_line_);
    size_t jmp = current_chunk_->emit_jump(Opcode::JUMP, current_line_);
    loop_stack_.back().break_offsets.push_back(jmp);
}

void Compiler::visit(ContinueStmt&) {
    if (loop_stack_.empty()) throw CompileError("'continue' outside loop", current_line_);
    // Emit a jump placeholder; will be patched after loop body is compiled
    size_t jmp = current_chunk_->emit_jump(Opcode::JUMP, current_line_);
    loop_stack_.back().continue_targets.push_back(jmp);
}

// ─────────────────────────────────────────────
//  Disassembler
// ─────────────────────────────────────────────
size_t Disassembler::disassemble_instruction(const Chunk& chunk, size_t offset, std::string& out) {
    std::ostringstream os;
    os << std::setw(6) << std::setfill('0') << offset << "  ";

    if (offset < chunk.lines.size())
        os << "L" << std::setw(4) << std::setfill(' ') << chunk.lines[offset] << "  ";

    uint8_t raw = chunk.code[offset];
    Opcode op = static_cast<Opcode>(raw);
    os << std::left << std::setw(18) << opcode_name(op);

    ++offset;
    switch (op) {
        case Opcode::PUSH_INT: {
            int64_t v = 0;
            for (int i = 0; i < 8; ++i) v = (v << 8) | chunk.code[offset++];
            os << " " << v;
            break;
        }
        case Opcode::PUSH_FLOAT: {
            uint64_t u = 0;
            for (int i = 0; i < 8; ++i) u = (u << 8) | chunk.code[offset++];
            double d; memcpy(&d, &u, 8);
            os << " " << d;
            break;
        }
        case Opcode::PUSH_BOOL:
            os << " " << (chunk.code[offset++] ? "true" : "false");
            break;
        case Opcode::PUSH_STRING: {
            uint16_t idx = (chunk.code[offset] << 8) | chunk.code[offset+1]; offset += 2;
            os << " #" << idx;
            if (idx < chunk.string_constants.size())
                os << " \"" << chunk.string_constants[idx] << "\"";
            break;
        }
        case Opcode::LOAD_VAR:
        case Opcode::STORE_VAR:
        case Opcode::DEFINE_VAR: {
            uint16_t idx = (chunk.code[offset] << 8) | chunk.code[offset+1]; offset += 2;
            os << " slot=" << idx;
            break;
        }
        case Opcode::LOAD_GLOBAL:
        case Opcode::STORE_GLOBAL: {
            uint16_t idx = (chunk.code[offset] << 8) | chunk.code[offset+1]; offset += 2;
            os << " #" << idx;
            if (idx < chunk.string_constants.size())
                os << " \"" << chunk.string_constants[idx] << "\"";
            break;
        }
        case Opcode::JUMP:
        case Opcode::JUMP_IF_FALSE:
        case Opcode::JUMP_IF_TRUE: {
            uint32_t target = 0;
            for (int i = 0; i < 4; ++i) target = (target << 8) | chunk.code[offset++];
            os << " -> " << target;
            break;
        }
        case Opcode::CALL:
            os << " argc=" << (int)chunk.code[offset++];
            break;
        case Opcode::MAKE_FUNC: {
            uint16_t idx = (chunk.code[offset] << 8) | chunk.code[offset+1]; offset += 2;
            os << " fn[" << idx << "]";
            break;
        }
        default: break;
    }
    out += os.str() + "\n";
    return offset;
}

std::string Disassembler::disassemble(const Chunk& chunk, const std::string& name) {
    std::string out;
    out += "══════════════════════════════════════\n";
    out += "  CHUNK: " + name + "\n";
    out += "══════════════════════════════════════\n";
    out += "  String constants:\n";
    for (size_t i = 0; i < chunk.string_constants.size(); ++i)
        out += "    [" + std::to_string(i) + "] = \"" + chunk.string_constants[i] + "\"\n";
    out += "  Instructions:\n";
    size_t off = 0;
    while (off < chunk.code.size())
        off = disassemble_instruction(chunk, off, out);
    return out;
}

} // namespace cvm
