#pragma once
#include "cvm_types.hpp"
#include "ast.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace cvm {

// ─────────────────────────────────────────────
//  Function prototype (compiled function)
// ─────────────────────────────────────────────
struct FunctionProto {
    std::string name;
    int arity = 0;
    Chunk chunk;
    std::vector<std::string> params;
};

// ─────────────────────────────────────────────
//  Symbol Table (scoped)
// ─────────────────────────────────────────────
struct Scope {
    std::unordered_map<std::string, uint16_t> locals;
    uint16_t next_slot = 0;
    bool is_loop = false;
    bool is_function = false;
};

// ─────────────────────────────────────────────
//  Compiler
// ─────────────────────────────────────────────
class Compiler : public ASTVisitor {
public:
    struct Result {
        Chunk                              main_chunk;
        std::vector<FunctionProto>         functions;
        std::unordered_map<std::string,int> fn_name_to_idx;
    };

    Result compile(ASTNode& ast);

private:
    Result result_;
    Chunk* current_chunk_ = nullptr;     // chunk being written to
    int    current_line_  = 0;

    // Scope stack
    std::vector<Scope> scopes_;
    bool in_function_ = false;

    // Loop control (break/continue patching)
    struct LoopContext {
        std::vector<size_t> break_offsets;
        std::vector<size_t> continue_targets;
        size_t loop_start = 0;
    };
    std::vector<LoopContext> loop_stack_;

    // Helpers
    void emit(Opcode op);
    void emit_int(int64_t v);
    void emit_float(double v);
    void emit_bool(bool v);
    void emit_string(const std::string& s);
    void emit_load(const std::string& name, int line);
    void emit_store(const std::string& name, int line);

    void push_scope(bool is_loop = false, bool is_fn = false);
    void pop_scope();
    uint16_t define_local(const std::string& name);
    std::optional<uint16_t> resolve_local(const std::string& name);

    // Visitor implementations
    void visit(LiteralExpr&)        override;
    void visit(IdentifierExpr&)     override;
    void visit(BinaryExpr&)         override;
    void visit(UnaryExpr&)          override;
    void visit(AssignExpr&)         override;
    void visit(CompoundAssignExpr&) override;
    void visit(CallExpr&)           override;
    void visit(InputExpr&)          override;
    void visit(ExprStmt&)           override;
    void visit(LetStmt&)            override;
    void visit(PrintStmt&)          override;
    void visit(BlockStmt&)          override;
    void visit(IfStmt&)             override;
    void visit(WhileStmt&)          override;
    void visit(ForStmt&)            override;
    void visit(FnStmt&)             override;
    void visit(ReturnStmt&)         override;
    void visit(BreakStmt&)          override;
    void visit(ContinueStmt&)       override;
    void visit(ProgramNode&)        override;
};

// ─────────────────────────────────────────────
//  Bytecode Disassembler
// ─────────────────────────────────────────────
class Disassembler {
public:
    static std::string disassemble(const Chunk& chunk, const std::string& name = "main");
    static size_t disassemble_instruction(const Chunk& chunk, size_t offset,
                                          std::string& out);
};

} // namespace cvm
