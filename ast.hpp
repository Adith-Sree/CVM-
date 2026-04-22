#pragma once
#include "cvm_types.hpp"
#include <memory>
#include <vector>
#include <string>
#include <variant>

namespace cvm {

// ─────────────────────────────────────────────
//  AST Node hierarchy
// ─────────────────────────────────────────────

struct ASTNode;
using ASTNodePtr = std::unique_ptr<ASTNode>;

// Expression nodes
struct LiteralExpr;
struct IdentifierExpr;
struct BinaryExpr;
struct UnaryExpr;
struct AssignExpr;
struct CompoundAssignExpr;
struct CallExpr;
struct InputExpr;

// Statement nodes
struct ExprStmt;
struct LetStmt;
struct PrintStmt;
struct BlockStmt;
struct IfStmt;
struct WhileStmt;
struct ForStmt;
struct FnStmt;
struct ReturnStmt;
struct BreakStmt;
struct ContinueStmt;
struct ProgramNode;

// ─── Visitor interface ───────────────────────
struct ASTVisitor {
    virtual ~ASTVisitor() = default;
    virtual void visit(LiteralExpr&)       = 0;
    virtual void visit(IdentifierExpr&)    = 0;
    virtual void visit(BinaryExpr&)        = 0;
    virtual void visit(UnaryExpr&)         = 0;
    virtual void visit(AssignExpr&)        = 0;
    virtual void visit(CompoundAssignExpr&)= 0;
    virtual void visit(CallExpr&)          = 0;
    virtual void visit(InputExpr&)         = 0;
    virtual void visit(ExprStmt&)          = 0;
    virtual void visit(LetStmt&)           = 0;
    virtual void visit(PrintStmt&)         = 0;
    virtual void visit(BlockStmt&)         = 0;
    virtual void visit(IfStmt&)            = 0;
    virtual void visit(WhileStmt&)         = 0;
    virtual void visit(ForStmt&)           = 0;
    virtual void visit(FnStmt&)            = 0;
    virtual void visit(ReturnStmt&)        = 0;
    virtual void visit(BreakStmt&)         = 0;
    virtual void visit(ContinueStmt&)      = 0;
    virtual void visit(ProgramNode&)       = 0;
};

struct ASTNode {
    int line = 0, col = 0;
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& v) = 0;
    virtual std::string node_type() const = 0;
};

// ─── Expression Nodes ────────────────────────

struct LiteralExpr : ASTNode {
    Value value;
    explicit LiteralExpr(Value v) : value(std::move(v)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "Literal"; }
};

struct IdentifierExpr : ASTNode {
    std::string name;
    explicit IdentifierExpr(std::string n) : name(std::move(n)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "Identifier"; }
};

struct BinaryExpr : ASTNode {
    std::string op;
    ASTNodePtr left, right;
    BinaryExpr(std::string o, ASTNodePtr l, ASTNodePtr r)
        : op(std::move(o)), left(std::move(l)), right(std::move(r)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "Binary"; }
};

struct UnaryExpr : ASTNode {
    std::string op;
    ASTNodePtr operand;
    UnaryExpr(std::string o, ASTNodePtr n) : op(std::move(o)), operand(std::move(n)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "Unary"; }
};

struct AssignExpr : ASTNode {
    std::string name;
    ASTNodePtr value;
    AssignExpr(std::string n, ASTNodePtr v) : name(std::move(n)), value(std::move(v)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "Assign"; }
};

struct CompoundAssignExpr : ASTNode {
    std::string name, op;
    ASTNodePtr value;
    CompoundAssignExpr(std::string n, std::string o, ASTNodePtr v)
        : name(std::move(n)), op(std::move(o)), value(std::move(v)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "CompoundAssign"; }
};

struct CallExpr : ASTNode {
    std::string callee;
    std::vector<ASTNodePtr> args;
    CallExpr(std::string c, std::vector<ASTNodePtr> a)
        : callee(std::move(c)), args(std::move(a)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "Call"; }
};

struct InputExpr : ASTNode {
    ASTNodePtr prompt; // optional prompt expression
    explicit InputExpr(ASTNodePtr p = nullptr) : prompt(std::move(p)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "Input"; }
};

// ─── Statement Nodes ─────────────────────────

struct ExprStmt : ASTNode {
    ASTNodePtr expr;
    explicit ExprStmt(ASTNodePtr e) : expr(std::move(e)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "ExprStmt"; }
};

struct LetStmt : ASTNode {
    std::string name;
    ASTNodePtr  initializer; // may be nullptr
    LetStmt(std::string n, ASTNodePtr i) : name(std::move(n)), initializer(std::move(i)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "Let"; }
};

struct PrintStmt : ASTNode {
    std::vector<ASTNodePtr> exprs;
    explicit PrintStmt(std::vector<ASTNodePtr> e) : exprs(std::move(e)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "Print"; }
};

struct BlockStmt : ASTNode {
    std::vector<ASTNodePtr> stmts;
    explicit BlockStmt(std::vector<ASTNodePtr> s) : stmts(std::move(s)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "Block"; }
};

struct IfStmt : ASTNode {
    ASTNodePtr condition;
    ASTNodePtr then_branch;
    ASTNodePtr else_branch; // may be nullptr
    IfStmt(ASTNodePtr c, ASTNodePtr t, ASTNodePtr e)
        : condition(std::move(c)), then_branch(std::move(t)), else_branch(std::move(e)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "If"; }
};

struct WhileStmt : ASTNode {
    ASTNodePtr condition;
    ASTNodePtr body;
    WhileStmt(ASTNodePtr c, ASTNodePtr b) : condition(std::move(c)), body(std::move(b)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "While"; }
};

struct ForStmt : ASTNode {
    ASTNodePtr init;       // let or expr; may be nullptr
    ASTNodePtr condition;  // may be nullptr (infinite)
    ASTNodePtr increment;  // may be nullptr
    ASTNodePtr body;
    ForStmt(ASTNodePtr i, ASTNodePtr c, ASTNodePtr inc, ASTNodePtr b)
        : init(std::move(i)), condition(std::move(c)), increment(std::move(inc)), body(std::move(b)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "For"; }
};

struct FnStmt : ASTNode {
    std::string name;
    std::vector<std::string> params;
    ASTNodePtr body;
    FnStmt(std::string n, std::vector<std::string> p, ASTNodePtr b)
        : name(std::move(n)), params(std::move(p)), body(std::move(b)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "Fn"; }
};

struct ReturnStmt : ASTNode {
    ASTNodePtr value; // may be nullptr
    explicit ReturnStmt(ASTNodePtr v) : value(std::move(v)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "Return"; }
};

struct BreakStmt : ASTNode {
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "Break"; }
};

struct ContinueStmt : ASTNode {
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "Continue"; }
};

struct ProgramNode : ASTNode {
    std::vector<ASTNodePtr> stmts;
    explicit ProgramNode(std::vector<ASTNodePtr> s) : stmts(std::move(s)) {}
    void accept(ASTVisitor& v) override { v.visit(*this); }
    std::string node_type() const override { return "Program"; }
};

// ─────────────────────────────────────────────
//  AST Pretty Printer
// ─────────────────────────────────────────────
class ASTPrinter : public ASTVisitor {
public:
    std::string output;
    void print(ASTNode& node) { node.accept(*this); }

private:
    int depth_ = 0;
    void indent() { output += std::string(depth_ * 2, ' '); }
    void enter(const std::string& label) { indent(); output += "(" + label; depth_++; }
    void leave() { depth_--; output += ")"; }
    void newline() { output += "\n"; }

public:
    void visit(LiteralExpr& n) override {
        indent(); output += "[Literal: " + value_to_string(n.value) + "]"; newline();
    }
    void visit(IdentifierExpr& n) override {
        indent(); output += "[Ident: " + n.name + "]"; newline();
    }
    void visit(BinaryExpr& n) override {
        enter("Binary " + n.op); newline();
        n.left->accept(*this);
        n.right->accept(*this);
        leave(); newline();
    }
    void visit(UnaryExpr& n) override {
        enter("Unary " + n.op); newline();
        n.operand->accept(*this);
        leave(); newline();
    }
    void visit(AssignExpr& n) override {
        enter("Assign " + n.name); newline();
        n.value->accept(*this);
        leave(); newline();
    }
    void visit(CompoundAssignExpr& n) override {
        enter("CompoundAssign " + n.name + " " + n.op); newline();
        n.value->accept(*this);
        leave(); newline();
    }
    void visit(CallExpr& n) override {
        enter("Call " + n.callee); newline();
        for (auto& a : n.args) a->accept(*this);
        leave(); newline();
    }
    void visit(InputExpr& n) override {
        enter("Input"); newline();
        if (n.prompt) n.prompt->accept(*this);
        leave(); newline();
    }
    void visit(ExprStmt& n) override { n.expr->accept(*this); }
    void visit(LetStmt& n) override {
        enter("Let " + n.name); newline();
        if (n.initializer) n.initializer->accept(*this);
        leave(); newline();
    }
    void visit(PrintStmt& n) override {
        enter("Print"); newline();
        for (auto& e : n.exprs) e->accept(*this);
        leave(); newline();
    }
    void visit(BlockStmt& n) override {
        enter("Block"); newline();
        for (auto& s : n.stmts) s->accept(*this);
        leave(); newline();
    }
    void visit(IfStmt& n) override {
        enter("If"); newline();
        n.condition->accept(*this);
        n.then_branch->accept(*this);
        if (n.else_branch) n.else_branch->accept(*this);
        leave(); newline();
    }
    void visit(WhileStmt& n) override {
        enter("While"); newline();
        n.condition->accept(*this);
        n.body->accept(*this);
        leave(); newline();
    }
    void visit(ForStmt& n) override {
        enter("For"); newline();
        if (n.init)      n.init->accept(*this);
        if (n.condition) n.condition->accept(*this);
        if (n.increment) n.increment->accept(*this);
        n.body->accept(*this);
        leave(); newline();
    }
    void visit(FnStmt& n) override {
        enter("Fn " + n.name + "(");
        for (size_t i = 0; i < n.params.size(); ++i) {
            output += n.params[i];
            if (i + 1 < n.params.size()) output += ", ";
        }
        output += ")"; newline();
        n.body->accept(*this);
        leave(); newline();
    }
    void visit(ReturnStmt& n) override {
        enter("Return"); newline();
        if (n.value) n.value->accept(*this);
        leave(); newline();
    }
    void visit(BreakStmt&) override { indent(); output += "[Break]\n"; }
    void visit(ContinueStmt&) override { indent(); output += "[Continue]\n"; }
    void visit(ProgramNode& n) override {
        enter("Program"); newline();
        for (auto& s : n.stmts) s->accept(*this);
        leave(); newline();
    }
};

} // namespace cvm
