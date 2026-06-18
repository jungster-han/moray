#ifndef MORAY_PARSER_H
#define MORAY_PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer   *lexer;
    Token    current;
    Token    previous;
    Token    pending;       /* one-token pushback buffer        */
    int      has_pending;   /* 1 when `pending` holds a token   */
    int      had_error;
} Parser;

void    parser_init(Parser *p, Lexer *l);
Program parser_parse(Parser *p);

#endif
