#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interpreter.h"

/* ── Signal type for unwinding the call stack on return ───────────── */
/*
 * When a return statement executes deep inside nested calls, we need to
 * stop executing statements and bubble the value back up to the function
 * that was called. We do this with a global signal rather than setjmp/longjmp.
 */
static int   g_returning = 0;
static Value g_return_val;

static void runtime_error(Interpreter *interp, int line, const char *msg) {
    fprintf(stderr, "[line %d] Runtime error: %s\n", line, msg);
    interp->had_error = 1;
}

/* ── Forward declarations ─────────────────────────────────────────── */
static Value eval_expr(Interpreter *interp, Expr *e, Env *env);
static void  exec_stmt(Interpreter *interp, Stmt *s, Env *env);

/* ── Built-in functions ───────────────────────────────────────────── */
static Value call_builtin(Interpreter *interp, const char *name,
                          Value *args, int argc, int line) {
    if (strcmp(name, "print") == 0) {
        for (int i = 0; i < argc; i++) {
            if (i > 0) printf(" ");
            value_print(args[i]);
        }
        printf("\n");
        return val_null();
    }
    if (strcmp(name, "type") == 0) {
        if (argc != 1) { runtime_error(interp, line, "type() takes 1 argument"); return val_null(); }
        return val_string(value_type_name(args[0].type), strlen(value_type_name(args[0].type)));
    }
    if (strcmp(name, "int") == 0) {
        if (argc != 1) { runtime_error(interp, line, "int() takes 1 argument"); return val_null(); }
        if (args[0].type == VAL_FLOAT)  return val_int((long)args[0].floating);
        if (args[0].type == VAL_INT)    return args[0];
        runtime_error(interp, line, "int() requires a number"); return val_null();
    }
    if (strcmp(name, "float") == 0) {
        if (argc != 1) { runtime_error(interp, line, "float() takes 1 argument"); return val_null(); }
        if (args[0].type == VAL_INT)    return val_float((double)args[0].integer);
        if (args[0].type == VAL_FLOAT)  return args[0];
        runtime_error(interp, line, "float() requires a number"); return val_null();
    }
    runtime_error(interp, line, "Undefined function");
    return val_null();
}

/* ── Expression evaluator ─────────────────────────────────────────── */
static Value eval_expr(Interpreter *interp, Expr *e, Env *env) {
    if (interp->had_error) return val_null();

    switch (e->kind) {
        case EXPR_NUM:  {
            /* store as int if it has no fractional part */
            double v = e->num;
            return (v == (long)v) ? val_int((long)v) : val_float(v);
        }
        case EXPR_STR:   return val_string(e->str.ptr, e->str.len);
        case EXPR_BOOL:  return val_bool(e->bool_val);
        case EXPR_NULL:  return val_null();

        case EXPR_IDENT: {
            Value v;
            if (!env_get(env, e->ident, &v)) {
                runtime_error(interp, e->line, "Undefined variable");
                fprintf(stderr, "  '%s'\n", e->ident);
            }
            return v;
        }

        case EXPR_UNARY: {
            Value right = eval_expr(interp, e->unary.right, env);
            if (strcmp(e->unary.op, "-") == 0) {
                if (right.type == VAL_INT)   return val_int(-right.integer);
                if (right.type == VAL_FLOAT) return val_float(-right.floating);
                runtime_error(interp, e->line, "Unary '-' requires a number");
            } else { /* not */
                return val_bool(!right.boolean);
            }
            return val_null();
        }

        case EXPR_BINARY: {
            Value left  = eval_expr(interp, e->binary.left,  env);
            Value right = eval_expr(interp, e->binary.right, env);
            const char *op = e->binary.op;

            /* logical */
            if (strcmp(op, "&&") == 0) return val_bool(left.boolean && right.boolean);
            if (strcmp(op, "||") == 0) return val_bool(left.boolean || right.boolean);

            /* equality works across all types */
            if (strcmp(op, "==") == 0) {
                if (left.type != right.type) return val_bool(0);
                switch (left.type) {
                    case VAL_INT:    return val_bool(left.integer  == right.integer);
                    case VAL_FLOAT:  return val_bool(left.floating == right.floating);
                    case VAL_BOOL:   return val_bool(left.boolean  == right.boolean);
                    case VAL_STRING: return val_bool(strcmp(left.string, right.string) == 0);
                    case VAL_NULL:   return val_bool(1);
                }
            }
            if (strcmp(op, "!=") == 0) {
                Value eq = eval_expr(interp, e, env); /* reuse == logic — but simpler inline: */
                if (left.type != right.type) return val_bool(1);
                switch (left.type) {
                    case VAL_INT:    return val_bool(left.integer  != right.integer);
                    case VAL_FLOAT:  return val_bool(left.floating != right.floating);
                    case VAL_BOOL:   return val_bool(left.boolean  != right.boolean);
                    case VAL_STRING: return val_bool(strcmp(left.string, right.string) != 0);
                    case VAL_NULL:   return val_bool(0);
                }
                (void)eq;
            }

            /* arithmetic — promote int to float if mixed */
            int  both_int   = left.type == VAL_INT   && right.type == VAL_INT;
            int  any_float  = left.type == VAL_FLOAT || right.type == VAL_FLOAT;
            double l = left.type  == VAL_INT ? (double)left.integer  : left.floating;
            double r = right.type == VAL_INT ? (double)right.integer : right.floating;

            if (any_float || both_int) {
                if (strcmp(op, "+") == 0) return both_int ? val_int((long)(l+r)) : val_float(l+r);
                if (strcmp(op, "-") == 0) return both_int ? val_int((long)(l-r)) : val_float(l-r);
                if (strcmp(op, "*") == 0) return both_int ? val_int((long)(l*r)) : val_float(l*r);
                if (strcmp(op, "/") == 0) {
                    if (r == 0) { runtime_error(interp, e->line, "Division by zero"); return val_null(); }
                    return val_float(l / r);
                }
                if (strcmp(op, "%") == 0) {
                    if (!both_int) { runtime_error(interp, e->line, "'%' requires integers"); return val_null(); }
                    if (right.integer == 0) { runtime_error(interp, e->line, "Modulo by zero"); return val_null(); }
                    return val_int(left.integer % right.integer);
                }
                if (strcmp(op, "<")  == 0) return val_bool(l <  r);
                if (strcmp(op, "<=") == 0) return val_bool(l <= r);
                if (strcmp(op, ">")  == 0) return val_bool(l >  r);
                if (strcmp(op, ">=") == 0) return val_bool(l >= r);
            }

            /* string concatenation */
            if (strcmp(op, "+") == 0 && left.type == VAL_STRING && right.type == VAL_STRING) {
                int  len = strlen(left.string) + strlen(right.string);
                char *s  = malloc(len + 1);
                strcpy(s, left.string);
                strcat(s, right.string);
                Value v = (Value){ VAL_STRING, .string = s };
                return v;
            }

            runtime_error(interp, e->line, "Invalid operands for operator");
            return val_null();
        }

        case EXPR_CALL: {
            /* check if it's a user-defined function in the environment */
            Value fn_val;
            if (env_get(env, e->call.name, &fn_val)) {
                /* user-defined functions stored as VAL_NULL with a side-channel
                   are handled via the fn_def lookup below — see exec_stmt */
            }

            /* evaluate arguments */
            Value args[64];
            int   argc = e->call.args.len;
            for (int i = 0; i < argc; i++)
                args[i] = eval_expr(interp, e->call.args.data[i], env);

            /* look up user-defined function in globals */
            Value fn;
            if (env_get(interp->globals, e->call.name, &fn)) {
                /* user function: fn.string holds a pointer to the Stmt (ugly but works) */
                Stmt *fn_stmt;
                memcpy(&fn_stmt, fn.string, sizeof(Stmt *));

                Env *fn_env = env_new(interp->globals);
                for (int i = 0; i < fn_stmt->fn_def.params.len && i < argc; i++)
                    env_define(fn_env, fn_stmt->fn_def.params.data[i].name, args[i]);

                exec_stmt(interp, fn_stmt->fn_def.body, fn_env);
                env_free(fn_env);

                Value ret = g_returning ? g_return_val : val_null();
                g_returning = 0;
                return ret;
            }

            return call_builtin(interp, e->call.name, args, argc, e->line);
        }
    }
    return val_null();
}

/* ── Statement executor ───────────────────────────────────────────── */
static void exec_stmt(Interpreter *interp, Stmt *s, Env *env) {
    if (interp->had_error || g_returning) return;

    switch (s->kind) {
        case STMT_VAR_DECL: {
            Value v = eval_expr(interp, s->var_decl.init, env);
            env_define(env, s->var_decl.name, v);
            break;
        }
        case STMT_ASSIGN: {
            Value v = eval_expr(interp, s->assign.value, env);
            if (!env_set(env, s->assign.name, v)) {
                runtime_error(interp, s->line, "Assignment to undefined variable");
                fprintf(stderr, "  '%s'\n", s->assign.name);
            }
            break;
        }
        case STMT_EXPR:
            eval_expr(interp, s->expr, env);
            break;

        case STMT_BLOCK: {
            Env *block_env = env_new(env);
            for (int i = 0; i < s->block.len && !g_returning; i++)
                exec_stmt(interp, s->block.data[i], block_env);
            env_free(block_env);
            break;
        }
        case STMT_IF: {
            Value cond = eval_expr(interp, s->if_stmt.condition, env);
            int truthy = (cond.type == VAL_BOOL)  ? cond.boolean :
                         (cond.type == VAL_INT)    ? cond.integer != 0 :
                         (cond.type == VAL_FLOAT)  ? cond.floating != 0.0 :
                         (cond.type == VAL_NULL)   ? 0 : 1;
            if (truthy)
                exec_stmt(interp, s->if_stmt.then_block, env);
            else if (s->if_stmt.else_block)
                exec_stmt(interp, s->if_stmt.else_block, env);
            break;
        }
        case STMT_WHILE: {
            for (;;) {
                Value cond = eval_expr(interp, s->while_stmt.condition, env);
                int truthy = (cond.type == VAL_BOOL)  ? cond.boolean :
                             (cond.type == VAL_INT)    ? cond.integer != 0 :
                             (cond.type == VAL_FLOAT)  ? cond.floating != 0.0 :
                             (cond.type == VAL_NULL)   ? 0 : 1;
                if (!truthy || interp->had_error) break;
                exec_stmt(interp, s->while_stmt.body, env);
                if (g_returning) break;
            }
            break;
        }
        case STMT_RETURN:
            g_return_val = s->ret.value
                ? eval_expr(interp, s->ret.value, env)
                : val_null();
            g_returning = 1;
            break;

        case STMT_FN_DEF: {
            /*
             * Store a pointer to the Stmt itself as the function's value.
             * We pack the pointer into a string-sized buffer — a simple trick
             * to avoid adding a new VAL_FUNCTION type for now.
             */
            char buf[sizeof(Stmt *)];
            memcpy(buf, &s, sizeof(Stmt *));
            Value fn = (Value){ VAL_STRING, .string = malloc(sizeof(Stmt *)) };
            memcpy(fn.string, &s, sizeof(Stmt *));
            env_define(interp->globals, s->fn_def.name, fn);
            break;
        }
    }
}

/* ── Entry points ─────────────────────────────────────────────────── */
void interpreter_init(Interpreter *interp) {
    interp->globals   = env_new(NULL);
    interp->had_error = 0;
}

void interpreter_free(Interpreter *interp) {
    env_free(interp->globals);
}

void interpreter_run(Interpreter *interp, Program *prog) {
    for (int i = 0; i < prog->stmts.len; i++) {
        exec_stmt(interp, prog->stmts.data[i], interp->globals);
        if (interp->had_error) break;
    }
}
