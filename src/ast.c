#include <stdlib.h>
#include <string.h>
#include "ast.h"

Expr *expr_alloc(ExprKind kind, int line) {
    Expr *e = calloc(1, sizeof(Expr));
    e->kind = kind;
    e->line = line;
    return e;
}

Stmt *stmt_alloc(StmtKind kind, int line) {
    Stmt *s = calloc(1, sizeof(Stmt));
    s->kind = kind;
    s->line = line;
    return s;
}

void expr_free(Expr *e) {
    if (!e) return;
    switch (e->kind) {
        case EXPR_IDENT:  free(e->ident); break;
        case EXPR_BINARY: expr_free(e->binary.left); expr_free(e->binary.right); break;
        case EXPR_UNARY:  expr_free(e->unary.right); break;
        case EXPR_CALL:
            free(e->call.name);
            for (int i = 0; i < e->call.args.len; i++)
                expr_free(e->call.args.data[i]);
            vector_free(&e->call.args);
            break;
        default: break;
    }
    free(e);
}

void stmt_free(Stmt *s) {
    if (!s) return;
    switch (s->kind) {
        case STMT_VAR_DECL:
            free(s->var_decl.name);
            expr_free(s->var_decl.init);
            break;
        case STMT_ASSIGN:
            free(s->assign.name);
            expr_free(s->assign.value);
            break;
        case STMT_IF:
            expr_free(s->if_stmt.condition);
            stmt_free(s->if_stmt.then_block);
            stmt_free(s->if_stmt.else_block);
            break;
        case STMT_WHILE:
            expr_free(s->while_stmt.condition);
            stmt_free(s->while_stmt.body);
            break;
        case STMT_RETURN: expr_free(s->ret.value); break;
        case STMT_FN_DEF:
            free(s->fn_def.name);
            for (int i = 0; i < s->fn_def.params.len; i++)
                free(s->fn_def.params.data[i].name);
            vector_free(&s->fn_def.params);
            stmt_free(s->fn_def.body);
            break;
        case STMT_EXPR: expr_free(s->expr); break;
        case STMT_BLOCK:
            for (int i = 0; i < s->block.len; i++)
                stmt_free(s->block.data[i]);
            vector_free(&s->block);
            break;
    }
    free(s);
}

void program_free(Program *p) {
    for (int i = 0; i < p->stmts.len; i++)
        stmt_free(p->stmts.data[i]);
    vector_free(&p->stmts);
}
