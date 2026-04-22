#include "lexer.hpp"
#include "parser.hpp"
#include "compiler.hpp"
#include "vm.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <functional>
#include <string>

// ─────────────────────────────────────────────
//  Minimal test framework
// ─────────────────────────────────────────────
struct TestCase {
    std::string name;
    std::string source;
    std::string expected_output; // what should be printed
    bool should_error = false;
};

struct TestResults {
    int passed = 0, failed = 0, total = 0;
};

static std::string run_cvm(const std::string& source) {
    std::ostringstream out;
    cvm::VM vm;
    vm.set_output(&out);

    cvm::Lexer lexer(source);
    auto tokens = lexer.tokenize();
    cvm::Parser parser(std::move(tokens));
    auto ast = parser.parse();
    cvm::Compiler compiler;
    auto compiled = compiler.compile(*ast);
    vm.execute(compiled);
    return out.str();
}

static bool run_test(const TestCase& tc, TestResults& results) {
    ++results.total;
    bool pass = false;
    std::string actual;

    try {
        actual = run_cvm(tc.source);
        pass = (!tc.should_error) && (actual == tc.expected_output);
    } catch (const std::exception& e) {
        if (tc.should_error) {
            pass = true;
        } else {
            actual = std::string("[EXCEPTION] ") + e.what();
            pass = false;
        }
    }

    if (pass) {
        std::cout << "  \033[32m✓\033[0m " << tc.name << "\n";
        ++results.passed;
    } else {
        std::cout << "  \033[31m✗\033[0m " << tc.name << "\n";
        if (!tc.should_error) {
            std::cout << "    Expected: " << tc.expected_output;
            std::cout << "    Got:      " << actual;
        }
        ++results.failed;
    }
    return pass;
}

int main() {
    std::cout << "\n\033[1;36m══ CVM++ Test Suite ══════════════════════\033[0m\n\n";

    std::vector<TestCase> tests = {
        // ── Basic printing ──────────────────────────────────────────
        {
            "Print integer",
            "print(42)",
            "42\n"
        },
        {
            "Print float",
            "print(3.14)",
            "3.14\n"
        },
        {
            "Print string",
            R"(print("hello"))",
            "hello\n"
        },
        {
            "Print bool true",
            "print(true)",
            "true\n"
        },
        {
            "Print bool false",
            "print(false)",
            "false\n"
        },
        {
            "Print nil",
            "print(nil)",
            "nil\n"
        },

        // ── Arithmetic ─────────────────────────────────────────────
        {
            "Addition",
            "print(1 + 2)",
            "3\n"
        },
        {
            "Subtraction",
            "print(10 - 4)",
            "6\n"
        },
        {
            "Multiplication",
            "print(3 * 7)",
            "21\n"
        },
        {
            "Integer division",
            "print(10 / 2)",
            "5\n"
        },
        {
            "Modulo",
            "print(10 % 3)",
            "1\n"
        },
        {
            "Unary minus",
            "print(-5)",
            "-5\n"
        },
        {
            "Nested arithmetic",
            "print((2 + 3) * (4 - 1))",
            "15\n"
        },
        {
            "Float arithmetic",
            "print(1.5 + 2.5)",
            "4.0\n"
        },

        // ── Variables ──────────────────────────────────────────────
        {
            "Variable declaration",
            "let x = 10\nprint(x)",
            "10\n"
        },
        {
            "Variable assignment",
            "let x = 5\nx = 20\nprint(x)",
            "20\n"
        },
        {
            "Multiple variables",
            "let a = 1\nlet b = 2\nprint(a + b)",
            "3\n"
        },
        {
            "Compound add-assign",
            "let x = 10\nx += 5\nprint(x)",
            "15\n"
        },
        {
            "Compound sub-assign",
            "let x = 10\nx -= 3\nprint(x)",
            "7\n"
        },
        {
            "Compound mul-assign",
            "let x = 4\nx *= 3\nprint(x)",
            "12\n"
        },

        // ── Comparison & Logic ─────────────────────────────────────
        {
            "Equal true",
            "print(5 == 5)",
            "true\n"
        },
        {
            "Equal false",
            "print(5 == 6)",
            "false\n"
        },
        {
            "Not equal",
            "print(5 != 6)",
            "true\n"
        },
        {
            "Less than",
            "print(3 < 5)",
            "true\n"
        },
        {
            "Greater than",
            "print(5 > 3)",
            "true\n"
        },
        {
            "LTE",
            "print(5 <= 5)",
            "true\n"
        },
        {
            "Logical AND",
            "print(true and false)",
            "false\n"
        },
        {
            "Logical OR",
            "print(false or true)",
            "true\n"
        },
        {
            "Logical NOT",
            "print(not true)",
            "false\n"
        },

        // ── If/else ────────────────────────────────────────────────
        {
            "If true branch",
            "if (true) { print(1) }",
            "1\n"
        },
        {
            "If false no output",
            "if (false) { print(1) }",
            ""
        },
        {
            "If-else true",
            "if (1 < 2) { print(\"yes\") } else { print(\"no\") }",
            "yes\n"
        },
        {
            "If-else false",
            "if (1 > 2) { print(\"yes\") } else { print(\"no\") }",
            "no\n"
        },
        {
            "Nested if",
            "let x = 5\nif (x > 0) { if (x < 10) { print(\"in range\") } }",
            "in range\n"
        },

        // ── While ──────────────────────────────────────────────────
        {
            "While loop",
            "let i = 0\nwhile (i < 3) { print(i)\ni = i + 1 }",
            "0\n1\n2\n"
        },
        {
            "While with break",
            "let i = 0\nwhile (true) { if (i == 3) { break }\nprint(i)\ni = i + 1 }",
            "0\n1\n2\n"
        },

        // ── For ────────────────────────────────────────────────────
        {
            "For loop basic",
            "for (let i = 0; i < 3; i = i + 1) { print(i) }",
            "0\n1\n2\n"
        },
        {
            "For loop sum",
            "let s = 0\nfor (let i = 1; i <= 5; i = i + 1) { s += i }\nprint(s)",
            "15\n"
        },

        // ── Functions ──────────────────────────────────────────────
        {
            "Function definition and call",
            "fn greet() { print(\"hello\") }\ngreet()",
            "hello\n"
        },
        {
            "Function with return",
            "fn add(a, b) { return a + b }\nprint(add(3, 4))",
            "7\n"
        },
        {
            "Recursive function (factorial)",
            "fn fact(n) { if (n <= 1) { return 1 }\nreturn n * fact(n - 1) }\nprint(fact(5))",
            "120\n"
        },
        {
            "Function with local scope",
            "let x = 100\nfn foo() { let x = 42\nprint(x) }\nfoo()\nprint(x)",
            "42\n100\n"
        },
        {
            "Multiple function calls",
            "fn double(x) { return x * 2 }\nprint(double(3))\nprint(double(5))",
            "6\n10\n"
        },

        // ── Stdlib ─────────────────────────────────────────────────
        {
            "sqrt native",
            "print(sqrt(16.0))",
            "4.0\n"
        },
        {
            "abs negative",
            "print(abs(-7))",
            "7\n"
        },
        {
            "max function",
            "print(max(3, 7))",
            "7\n"
        },
        {
            "min function",
            "print(min(3, 7))",
            "3\n"
        },
        {
            "str() conversion",
            "print(str(42))",
            "42\n"
        },
        {
            "int() conversion",
            R"(print(int("123")))",
            "123\n"
        },
        {
            "type() function",
            "print(type(42))",
            "int\n"
        },
        {
            "len() on string",
            R"(print(len("hello")))",
            "5\n"
        },

        // ── String ops ─────────────────────────────────────────────
        {
            "String concatenation with +",
            R"(print("hello" + " " + "world"))",
            "hello world\n"
        },
        {
            "String repeat",
            R"(print("ab" * 3))",
            "ababab\n"
        },

        // ── Error handling ─────────────────────────────────────────
        {
            "Division by zero (should error)",
            "print(1 / 0)",
            "",
            true
        },
        {
            "Unknown variable (nil)",
            "print(xyz)",
            "nil\n"
        },
    };

    TestResults results;
    std::string current_section;

    auto sections = {
        std::make_pair("Basic Printing",    0),
        std::make_pair("Arithmetic",        6),
        std::make_pair("Variables",        14),
        std::make_pair("Comparison & Logic",20),
        std::make_pair("Control Flow",     29),
        std::make_pair("Loops",            34),
        std::make_pair("Functions",        37),
        std::make_pair("Standard Library", 43),
        std::make_pair("String Ops",       50),
        std::make_pair("Error Handling",   52),
    };

    int idx = 0;
    int section_idx = 0;
    auto section_list = sections.begin();
    auto section_next = sections.begin(); ++section_next;

    for (int i = 0; i < (int)tests.size(); ++i) {
        for (auto it = sections.begin(); it != sections.end(); ++it) {
            if (it->second == i) {
                std::cout << "\n\033[1;33m── " << it->first << " ──\033[0m\n";
                break;
            }
        }
        run_test(tests[i], results);
    }

    std::cout << "\n\033[1;36m══ Results ═══════════════════════════════\033[0m\n";
    std::cout << "  Total  : " << results.total  << "\n";
    std::cout << "  \033[32mPassed : " << results.passed << "\033[0m\n";
    std::cout << "  \033[31mFailed : " << results.failed << "\033[0m\n";
    std::cout << "  Score  : " << (results.passed * 100 / (results.total ? results.total : 1)) << "%\n\n";

    return results.failed > 0 ? 1 : 0;
}
