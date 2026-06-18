#ifndef MORAY_ENV_H
#define MORAY_ENV_H

#include "value.h"

#define ENV_MAX_VARS 256

typedef struct Env Env;

struct Env {
    char  *names[ENV_MAX_VARS];
    Value  values[ENV_MAX_VARS];
    int    count;
    Env   *parent;   /* enclosing scope — NULL for global */
};

Env  *env_new(Env *parent);   /* allocate a new scope */
void  env_free(Env *e);       /* free scope (not its parent) */

/* Returns 1 on success, 0 if the scope is full */
int   env_define(Env *e, const char *name, Value val);

/* Walks up the scope chain. Returns 1 and sets *out if found */
int   env_get(Env *e, const char *name, Value *out);

/* Walks up the chain and updates existing variable. Returns 1 if found */
int   env_set(Env *e, const char *name, Value val);

#endif
