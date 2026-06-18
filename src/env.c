#include <stdlib.h>
#include <string.h>
#include "env.h"

Env *env_new(Env *parent) {
    Env *e   = calloc(1, sizeof(Env));
    e->parent = parent;
    return e;
}

void env_free(Env *e) {
    for (int i = 0; i < e->count; i++) {
        free(e->names[i]);
        value_free(e->values[i]);
    }
    free(e);
}

int env_define(Env *e, const char *name, Value val) {
    if (e->count >= ENV_MAX_VARS) return 0;
    e->names[e->count]  = strdup(name);
    e->values[e->count] = val;
    e->count++;
    return 1;
}

int env_get(Env *e, const char *name, Value *out) {
    /* search current scope first, then walk up to parent */
    for (Env *scope = e; scope != NULL; scope = scope->parent) {
        for (int i = 0; i < scope->count; i++) {
            if (strcmp(scope->names[i], name) == 0) {
                *out = scope->values[i];
                return 1;
            }
        }
    }
    return 0;
}

int env_set(Env *e, const char *name, Value val) {
    for (Env *scope = e; scope != NULL; scope = scope->parent) {
        for (int i = 0; i < scope->count; i++) {
            if (strcmp(scope->names[i], name) == 0) {
                value_free(scope->values[i]);
                scope->values[i] = val;
                return 1;
            }
        }
    }
    return 0;
}
