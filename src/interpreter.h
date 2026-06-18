#ifndef MORAY_INTERPRETER_H
#define MORAY_INTERPRETER_H

#include "ast.h"
#include "value.h"
#include "env.h"

typedef struct {
    Env *globals;
    int  had_error;
} Interpreter;

void  interpreter_init(Interpreter *interp);
void  interpreter_free(Interpreter *interp);
void  interpreter_run(Interpreter *interp, Program *prog);

#endif
