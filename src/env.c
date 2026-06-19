#include <stdlib.h>
#include <string.h>
#include "env.h"

/* Registry of every live scope, used as the GC root set. A scope joins on
   creation and leaves on teardown, so the collector can reach every variable
   currently in play. */
static Env *g_live_envs = NULL;

void env_gc_mark_roots(void) {
    for (Env *e = g_live_envs; e != NULL; e = e->gc_link)
        for (int i = 0; i < e->count; i++)
            gc_mark_value(e->values[i]);
}

Env *env_new(Env *parent) {
    Env *e      = calloc(1, sizeof(Env));
    e->parent   = parent;
    e->gc_link  = g_live_envs;   /* register as a live root */
    g_live_envs = e;
    return e;
}

void env_free(Env *e) {
    /* Unlink from the root registry first: once a scope is gone its variables
       are no longer roots, so any object reachable only through it becomes
       collectible. */
    if (g_live_envs == e) {
        g_live_envs = e->gc_link;
    } else {
        for (Env *p = g_live_envs; p != NULL; p = p->gc_link) {
            if (p->gc_link == e) { p->gc_link = e->gc_link; break; }
        }
    }

    /* The names are private to this scope; free them. The values are shared by
       handle and owned by the collector, so we never free them here — we just
       drop our references and let a collection reclaim whatever is now
       unreachable. */
    for (int i = 0; i < e->count; i++)
        free(e->names[i]);
    free(e);

    gc_maybe_collect();
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
