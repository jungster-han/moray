#ifndef MORAY_AST_H
#define MORAY_AST_H

#include "vec.h"

/* ── Expression kinds ─────────────────────────────────────────────── */
typedef enum {
    EXPR_NUM,       /* 3.14          */
    EXPR_STR,       /* "hello"       */
    EXPR_BOOL,      /* true / false  */
    EXPR_NULL,      /* null          */
    EXPR_IDENT,     /* x             */
    EXPR_BINARY,    /* a + b         */
    EXPR_UNARY,     /* not x, -x     */
    EXPR_CALL,      /* add(1, 2)     */
    EXPR_LIST,      /* [1, 2, 3]     */
    EXPR_MAP,       /* {"a": 1}      */
    EXPR_INDEX,     /* x[0], m["k"]  */
} ExprKind;

/* ── Statement kinds ──────────────────────────────────────────────── */
typedef enum {
    STMT_VAR_DECL,  /* int x = 10    */
    STMT_ASSIGN,    /* x = 20        */
    STMT_IF,        /* if cond { }   */
    STMT_WHILE,     /* while cond {} */
    STMT_RETURN,    /* return expr   */
    STMT_FN_DEF,    /* fn foo(a) {}  */
    STMT_EXPR,      /* bare expr     */
    STMT_BLOCK,     /* { stmts }     */
} StmtKind;

/* ── Type annotation ──────────────────────────────────────────────── */
typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_VOID,      /* for functions that don't return a value */
    TYPE_LIST,
    TYPE_MAP,
} MorayType;

/* forward declare so Expr and Stmt can reference each other */
typedef struct Expr Expr;
typedef struct Stmt Stmt;

/* ── Vector element types ──────────────────────────────────────────── */
/*
 * Pointer aliases for the vector macros (element types must be a single
 * identifier, since the name is pasted into the generated struct name).
 * These, and the vector_define() calls, must come before any struct that
 * embeds them so the generated vec_* types are complete at point of use.
 */
typedef Expr *ExprPtr;
typedef Stmt *StmtPtr;

typedef struct {
    Expr *key;   /* always a string expression */
    Expr *value;
} MapEntry;

typedef struct {
    MorayType type;
    char *name;
} Param;

vector_define(ExprPtr)
vector_define(StmtPtr)
vector_define(Param)
vector_define(MapEntry)

/* ── Expressions ──────────────────────────────────────────────────── */
struct Expr {
    ExprKind kind;
    int line;

    union {
        double num;                     /* EXPR_NUM              */

        struct {                        /* EXPR_STR              */
            const char *ptr;
            int len;
        } str;

        int bool_val;                   /* EXPR_BOOL: 1 or 0     */

        char *ident;                    /* EXPR_IDENT            */

        struct {                        /* EXPR_BINARY           */
            Expr *left;
            char op[3];                 /* "+", "!=", "<=" etc.  */
            Expr *right;
        } binary;

        struct {                        /* EXPR_UNARY            */
            char op[4];                 /* "-" or "not"          */
            Expr *right;
        } unary;

        struct {                        /* EXPR_CALL             */
            char *name;
            vector(ExprPtr) args;
        } call;

        vector(ExprPtr) list;           /* EXPR_LIST             */

        vector(MapEntry) map;           /* EXPR_MAP              */

        struct {                        /* EXPR_INDEX            */
            Expr *object;               /* the list or map       */
            Expr *index;                /* the key or position   */
        } index;
    };
};

/* ── Statements ───────────────────────────────────────────────────── */

struct Stmt {
    StmtKind kind;
    int line;

    union {
        struct {                        /* STMT_VAR_DECL         */
            MorayType type;
            char *name;
            Expr *init;
        } var_decl;

        struct {                        /* STMT_ASSIGN           */
            char *name;
            Expr *value;
        } assign;

        struct {                        /* STMT_IF               */
            Expr *condition;
            Stmt *then_block;
            Stmt *else_block;           /* NULL if no else       */
        } if_stmt;

        struct {                        /* STMT_WHILE            */
            Expr *condition;
            Stmt *body;
        } while_stmt;

        struct {                        /* STMT_RETURN           */
            Expr *value;                /* NULL for bare return  */
        } ret;

        struct {                        /* STMT_FN_DEF           */
            char *name;
            vector(Param) params;
            Stmt *body;
        } fn_def;

        Expr *expr;                     /* STMT_EXPR             */

        vector(StmtPtr) block;          /* STMT_BLOCK            */
    };
};

/* ── Program ──────────────────────────────────────────────────────── */

typedef struct {
    vector(StmtPtr) stmts;
} Program;

/* Helpers */
Expr *expr_alloc(ExprKind kind, int line);
Stmt *stmt_alloc(StmtKind kind, int line);
void  expr_free(Expr *e);
void  stmt_free(Stmt *s);
void  program_free(Program *p);

#endif
