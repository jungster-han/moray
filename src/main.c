#include <stdio.h>
#include "lexer.h"
#include "parser.h"
#include "interpreter.h"

int main(void) {
    const char *source =
        "int x = 10\n"
        "int y = 3\n"
        "string greeting = \"Hello from Moray!\"\n"
        "\n"
        "fn add(int a, int b) {\n"
        "    return a + b\n"
        "}\n"
        "\n"
        "fn factorial(int n) {\n"
        "    if n <= 1 {\n"
        "        return 1\n"
        "    }\n"
        "    return n * factorial(n - 1)\n"
        "}\n"
        "\n"
        "print(greeting)\n"
        "print(add(x, y))\n"
        "print(factorial(6))\n"
        "\n"
        "int i = 0\n"
        "while i < 5 {\n"
        "    print(i)\n"
        "    i = i + 1\n"
        "}\n";

    printf("=== Running Moray ===\n\n");

    Lexer       lexer;   lexer_init(&lexer, source);
    Parser      parser;  parser_init(&parser, &lexer);
    Program     prog   = parser_parse(&parser);

    if (parser.had_error) { fprintf(stderr, "Parsing failed.\n"); return 1; }

    Interpreter interp;
    interpreter_init(&interp);
    interpreter_run(&interp, &prog);
    interpreter_free(&interp);
    program_free(&prog);
    return interp.had_error ? 1 : 0;
}
