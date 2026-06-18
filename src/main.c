#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "parser.h"
#include "interpreter.h"

/* Read an entire file into a newly allocated, null-terminated buffer. */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Could not open file '%s'.\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) { fclose(f); return NULL; }
    rewind(f);

    char *buffer = malloc((size_t)size + 1);
    if (!buffer) {
        fprintf(stderr, "Not enough memory to read '%s'.\n", path);
        fclose(f);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)size, f);
    buffer[read] = '\0';
    fclose(f);
    return buffer;
}

int main(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : "examples/sample.my";

    char *source = read_file(path);
    if (!source) return 1;

    printf("=== Running Moray (%s) ===\n\n", path);

    Lexer       lexer;   lexer_init(&lexer, source);
    Parser      parser;  parser_init(&parser, &lexer);
    Program     prog   = parser_parse(&parser);

    if (parser.had_error) {
        fprintf(stderr, "Parsing failed.\n");
        free(source);
        return 1;
    }

    Interpreter interp;
    interpreter_init(&interp);
    interpreter_run(&interp, &prog);
    interpreter_free(&interp);
    program_free(&prog);
    free(source);
    return interp.had_error ? 1 : 0;
}
