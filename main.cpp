#include "lexer.hpp"
#include "parser.hpp"
#include "compiler.hpp"
#include "vm.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>

// ─────────────────────────────────────────────
//  ANSI color codes
// ─────────────────────────────────────────────
namespace Color {
    const char* RESET   = "\033[0m";
    const char* BOLD    = "\033[1m";
    const char* DIM     = "\033[2m";
    const char* RED     = "\033[31m";
    const char* GREEN   = "\033[32m";
    const char* YELLOW  = "\033[33m";
    const char* BLUE    = "\033[34m";
    const char* MAGENTA = "\033[35m";
    const char* CYAN    = "\033[36m";
    const char* WHITE   = "\033[37m";
    const char* BRIGHT_RED    = "\033[91m";
    const char* BRIGHT_GREEN  = "\033[92m";
    const char* BRIGHT_YELLOW = "\033[93m";
    const char* BRIGHT_BLUE   = "\033[94m";
    const char* BRIGHT_CYAN   = "\033[96m";
    const char* BRIGHT_WHITE  = "\033[97m";
}

// ─────────────────────────────────────────────
//  CLI Options
// ─────────────────────────────────────────────
struct CLIOptions {
    std::string filename;
    bool repl         = false;
    bool debug_ast    = false;
    bool debug_bytecode = false;
    bool debug_stats  = false;
    bool no_color     = false;
    bool run_tests    = false;
    bool benchmark    = false;
};

// ─────────────────────────────────────────────
//  Pipeline: source → tokens → AST → bytecode → exec
// ─────────────────────────────────────────────
struct PipelineResult {
    bool ok = true;
    std::string error;
    int error_line = -1;
    std::string tokens_dump;
    std::string ast_dump;
    std::string bytecode_dump;
    cvm::VMResult vm_result;
    double compile_ms = 0;
    double exec_ms    = 0;
};

PipelineResult run_pipeline(const std::string& source,
                             cvm::VM& vm,
                             const CLIOptions& opts,
                             bool repl_mode = false) {
    PipelineResult result;

    try {
        // ─ 1. Lex ──────────────────────────
        auto t0 = std::chrono::high_resolution_clock::now();

        cvm::Lexer lexer(source);
        auto tokens = lexer.tokenize();

        if (opts.debug_bytecode) { // reuse flag for token dump
            std::ostringstream oss;
            oss << Color::DIM << "Tokens:\n" << Color::RESET;
            for (auto& t : tokens)
                oss << Color::DIM << "  [" << cvm::token_type_name(t.type)
                    << "] '" << t.lexeme << "' L" << t.line << "\n" << Color::RESET;
            result.tokens_dump = oss.str();
        }

        // ─ 2. Parse ────────────────────────
        cvm::Parser parser(std::move(tokens));
        auto ast = parser.parse();

        if (opts.debug_ast) {
            cvm::ASTPrinter printer;
            printer.print(*ast);
            result.ast_dump = printer.output;
        }

        // ─ 3. Compile ──────────────────────
        cvm::Compiler compiler;
        auto compiled = compiler.compile(*ast);

        auto t1 = std::chrono::high_resolution_clock::now();
        result.compile_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        if (opts.debug_bytecode) {
            std::string dis = cvm::Disassembler::disassemble(compiled.main_chunk, "main");
            for (auto& fn : compiled.functions)
                dis += cvm::Disassembler::disassemble(fn.chunk, fn.name);
            result.bytecode_dump = dis;
        }

        // ─ 4. Execute ──────────────────────
        auto t2 = std::chrono::high_resolution_clock::now();
        result.vm_result = vm.execute(compiled);
        auto t3 = std::chrono::high_resolution_clock::now();
        result.exec_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();

    } catch (const cvm::LexError& e) {
        result.ok = false;
        result.error_line = e.line;
        result.error = std::string("Lexer Error: ") + e.what();
    } catch (const cvm::ParseError& e) {
        result.ok = false;
        result.error_line = e.line;
        result.error = std::string("Parse Error: ") + e.what();
    } catch (const cvm::CompileError& e) {
        result.ok = false;
        result.error_line = e.line;
        result.error = std::string("Compile Error: ") + e.what();
    } catch (const cvm::RuntimeError& e) {
        result.ok = false;
        result.error = std::string("Runtime Error: ") + e.what();
    } catch (const std::exception& e) {
        result.ok = false;
        result.error = std::string("Error: ") + e.what();
    }

    return result;
}

// ─────────────────────────────────────────────
//  Banner
// ─────────────────────────────────────────────
void print_banner() {
    std::cout << Color::BRIGHT_CYAN << R"(
  ██████╗██╗   ██╗███╗   ███╗     ██╗  ██╗
 ██╔════╝██║   ██║████╗ ████║    ██╔╝ ██╔╝
 ██║     ██║   ██║██╔████╔██║   ██╔╝ ██╔╝ 
 ██║     ╚██╗ ██╔╝██║╚██╔╝██║  ██╔╝ ██╔╝  
 ╚██████╗ ╚████╔╝ ██║ ╚═╝ ██║ ██╔╝ ██╔╝   
  ╚═════╝  ╚═══╝  ╚═╝     ╚═╝╚═╝  ╚═╝    
)" << Color::RESET;
    std::cout << Color::BOLD << Color::BRIGHT_WHITE
              << "  Stack-Based Virtual Machine & Custom Compiler\n"
              << Color::RESET;
    std::cout << Color::DIM
              << "  Language: CVM Script  |  v1.0.0  |  C++17\n"
              << "  Type ':help' for commands, ':quit' to exit\n\n"
              << Color::RESET;
}

// ─────────────────────────────────────────────
//  REPL
// ─────────────────────────────────────────────
void run_repl(cvm::VM& vm, CLIOptions opts) {
    print_banner();

    int line_num = 1;
    std::string multiline_buffer;
    bool in_multiline = false;

    while (true) {
        // Prompt
        if (in_multiline)
            std::cout << Color::YELLOW << "  ... " << Color::RESET;
        else
            std::cout << Color::BRIGHT_CYAN << "cvm" << Color::RESET
                      << Color::DIM << "[" << line_num << "]" << Color::RESET
                      << Color::BRIGHT_CYAN << "> " << Color::RESET;

        std::string line;
        if (!std::getline(std::cin, line)) break;

        // REPL commands
        if (!in_multiline) {
            if (line == ":quit" || line == ":q" || line == ":exit") break;
            if (line == ":help" || line == ":h") {
                std::cout << Color::CYAN << "\nREPL Commands:\n" << Color::RESET
                    << "  :quit / :q      Exit the REPL\n"
                    << "  :help / :h      Show this help\n"
                    << "  :ast            Toggle AST debug output\n"
                    << "  :bytecode       Toggle bytecode disassembly\n"
                    << "  :stats          Toggle execution statistics\n"
                    << "  :globals        Show all global variables\n"
                    << "  :clear          Clear global state\n"
                    << "  :load <file>    Load and run a .cvm file\n\n"
                    << Color::DIM << "Multi-line: end line with '\\' to continue\n\n" << Color::RESET;
                continue;
            }
            if (line == ":ast") { opts.debug_ast = !opts.debug_ast;
                std::cout << Color::YELLOW << "AST debug: " << (opts.debug_ast ? "ON" : "OFF") << "\n" << Color::RESET; continue; }
            if (line == ":bytecode") { opts.debug_bytecode = !opts.debug_bytecode;
                std::cout << Color::YELLOW << "Bytecode debug: " << (opts.debug_bytecode ? "ON" : "OFF") << "\n" << Color::RESET; continue; }
            if (line == ":stats") { opts.debug_stats = !opts.debug_stats;
                std::cout << Color::YELLOW << "Stats: " << (opts.debug_stats ? "ON" : "OFF") << "\n" << Color::RESET; continue; }
            if (line == ":clear") {
                vm.reset_globals(); vm.reset_stats();
                std::cout << Color::GREEN << "Global state cleared.\n" << Color::RESET; continue; }
            if (line == ":globals") {
                auto& g = vm.globals();
                if (g.empty()) { std::cout << Color::DIM << "(no globals)\n" << Color::RESET; }
                else {
                    std::cout << Color::CYAN << "Globals:\n" << Color::RESET;
                    for (auto& [k, v] : g)
                        std::cout << "  " << Color::BRIGHT_WHITE << k << Color::RESET
                                  << " = " << Color::BRIGHT_YELLOW << cvm::value_to_string(v)
                                  << Color::RESET << "\n";
                }
                continue;
            }
            if (line.rfind(":load ", 0) == 0) {
                std::string fname = line.substr(6);
                std::ifstream f(fname);
                if (!f) { std::cout << Color::RED << "Cannot open: " << fname << "\n" << Color::RESET; continue; }
                std::string src((std::istreambuf_iterator<char>(f)), {});
                auto res = run_pipeline(src, vm, opts);
                if (!res.ok) std::cout << Color::BRIGHT_RED << res.error << "\n" << Color::RESET;
                else std::cout << Color::GREEN << "Loaded: " << fname << "\n" << Color::RESET;
                continue;
            }
            if (line.empty()) continue;
        }

        // Multi-line support
        if (!line.empty() && line.back() == '\\') {
            multiline_buffer += line.substr(0, line.size()-1) + "\n";
            in_multiline = true;
            continue;
        }
        if (in_multiline) {
            multiline_buffer += line;
            line = std::move(multiline_buffer);
            multiline_buffer.clear();
            in_multiline = false;
        }

        // Run
        auto res = run_pipeline(line, vm, opts, true);

        if (opts.debug_ast && !res.ast_dump.empty()) {
            std::cout << Color::DIM << "\n─── AST ───\n" << Color::RESET
                      << Color::MAGENTA << res.ast_dump << Color::RESET;
        }
        if (opts.debug_bytecode && !res.bytecode_dump.empty()) {
            std::cout << Color::DIM << "\n─── Bytecode ───\n" << Color::RESET
                      << Color::BLUE << res.bytecode_dump << Color::RESET;
        }
        if (res.ok) {
            if (!std::holds_alternative<std::monostate>(res.vm_result.retval)) {
                // In REPL, show last expression result
            }
            if (opts.debug_stats) {
                auto& s = vm.stats();
                std::cout << Color::DIM
                    << "  [compile: " << res.compile_ms << "ms"
                    << "  exec: " << res.exec_ms << "ms"
                    << "  instructions: " << s.instructions_executed
                    << "  stack_peak: " << s.stack_peak
                    << "  calls: " << s.function_calls << "]\n"
                    << Color::RESET;
            }
        } else {
            std::cout << Color::BRIGHT_RED << "✗ ";
            if (res.error_line > 0)
                std::cout << "Line " << res.error_line << ": ";
            std::cout << res.error << Color::RESET << "\n";
        }
        ++line_num;
    }
    std::cout << Color::DIM << "\nBye! 👋\n" << Color::RESET;
}

// ─────────────────────────────────────────────
//  File runner
// ─────────────────────────────────────────────
int run_file(const std::string& filename, cvm::VM& vm, const CLIOptions& opts) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << Color::BRIGHT_RED << "Error: Cannot open file '" << filename << "'\n" << Color::RESET;
        return 1;
    }
    std::string source((std::istreambuf_iterator<char>(file)), {});

    std::cout << Color::DIM << "╔══ CVM++ ═══════════════════════════════╗\n"
              << "  Running: " << filename << "\n"
              << "╚════════════════════════════════════════╝\n" << Color::RESET;

    auto res = run_pipeline(source, vm, opts);

    if (opts.debug_ast && !res.ast_dump.empty()) {
        std::cout << Color::MAGENTA << "\n─── AST ───────────────────────────────\n"
                  << res.ast_dump << Color::RESET;
    }
    if (opts.debug_bytecode && !res.bytecode_dump.empty()) {
        std::cout << Color::BLUE << "\n─── Bytecode ──────────────────────────\n"
                  << res.bytecode_dump << Color::RESET;
    }
    if (!res.ok) {
        std::cerr << Color::BRIGHT_RED << "\n✗ ";
        if (res.error_line > 0) std::cerr << "Line " << res.error_line << ": ";
        std::cerr << res.error << "\n" << Color::RESET;
        return 1;
    }
    if (opts.debug_stats) {
        auto& s = vm.stats();
        std::cout << Color::DIM
            << "\n─── Execution Stats ───────────────────\n"
            << "  Compile time   : " << res.compile_ms << " ms\n"
            << "  Execution time : " << res.exec_ms    << " ms\n"
            << "  Instructions   : " << s.instructions_executed << "\n"
            << "  Stack peak     : " << s.stack_peak << "\n"
            << "  Function calls : " << s.function_calls << "\n"
            << Color::RESET;
    }
    return 0;
}

// ─────────────────────────────────────────────
//  Help
// ─────────────────────────────────────────────
void print_help(const char* prog) {
    std::cout << Color::BOLD << "CVM++ " << Color::RESET << "— Stack-Based VM & Custom Compiler\n\n"
        << Color::BOLD << "USAGE:\n" << Color::RESET
        << "  " << prog << " [options] [file.cvm]\n\n"
        << Color::BOLD << "OPTIONS:\n" << Color::RESET
        << "  --repl           Launch interactive REPL (default if no file given)\n"
        << "  --ast            Print AST in debug mode\n"
        << "  --bytecode       Print disassembled bytecode\n"
        << "  --stats          Print execution statistics\n"
        << "  --no-color       Disable color output\n"
        << "  --help, -h       Show this help message\n\n"
        << Color::BOLD << "EXAMPLES:\n" << Color::RESET
        << "  " << prog << " script.cvm\n"
        << "  " << prog << " --bytecode --stats script.cvm\n"
        << "  " << prog << " --repl\n"
        << "  " << prog << " --ast --bytecode script.cvm\n\n"
        << Color::DIM << "Language docs: see README.md\n" << Color::RESET;
}

// ─────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────
int main(int argc, char* argv[]) {
    CLIOptions opts;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--repl")      { opts.repl = true; }
        else if (arg == "--ast")       { opts.debug_ast = true; }
        else if (arg == "--bytecode")  { opts.debug_bytecode = true; }
        else if (arg == "--stats")     { opts.debug_stats = true; }
        else if (arg == "--no-color")  { opts.no_color = true; }
        else if (arg == "--help" || arg == "-h") { print_help(argv[0]); return 0; }
        else if (!arg.empty() && arg[0] != '-') { opts.filename = arg; }
        else {
            std::cerr << "Unknown option: " << arg << "\nTry --help\n";
            return 1;
        }
    }

    if (opts.no_color) {
        Color::RESET = Color::BOLD = Color::DIM = "";
        Color::RED = Color::GREEN = Color::YELLOW = "";
        Color::BLUE = Color::MAGENTA = Color::CYAN = Color::WHITE = "";
        Color::BRIGHT_RED = Color::BRIGHT_GREEN = Color::BRIGHT_YELLOW = "";
        Color::BRIGHT_BLUE = Color::BRIGHT_CYAN = Color::BRIGHT_WHITE = "";
    }

    cvm::VM vm;

    if (!opts.filename.empty()) {
        return run_file(opts.filename, vm, opts);
    } else {
        run_repl(vm, opts);
        return 0;
    }
}
