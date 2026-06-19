#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "value.h"
#include "env.h"   /* for env_gc_mark_roots — the variable root set */

/* ── Garbage collector ─────────────────────────────────────────────────
 *
 * A simple mark-and-sweep collector. Every heap object is threaded on the
 * intrusive list `g_objects` at allocation; a collection marks everything
 * reachable from the roots (live scopes + the protect stack) and frees the
 * rest. See value.h for the rationale and the trigger policy.
 */

static GCHeader *g_objects = NULL;   /* all live heap objects                 */
static long      g_obj_count = 0;    /* objects currently tracked             */
static long      g_threshold = 256;  /* collect once this many objects exist  */

/* Protect stack — pins in-flight temporaries that no scope references yet. */
static Value *g_protect      = NULL;
static int    g_protect_len  = 0;
static int    g_protect_cap  = 0;

/* Recover the embedded/prefixed header from any heap object. The header is the
   first member of list/map/struct, and a prefix in front of string data. */
static GCHeader *list_header(MorayList *l)   { return &l->gc; }
static GCHeader *map_header(MorayMap *m)     { return &m->gc; }
static GCHeader *struct_header(MorayStruct *s){ return &s->gc; }
static GCHeader *string_header(char *s)      { return (GCHeader *)s - 1; }

/* Thread a freshly allocated object onto the all-objects list. */
static void gc_register(GCHeader *h, GCKind kind) {
    h->kind   = (unsigned char)kind;
    h->mark   = 0;
    h->next   = g_objects;
    g_objects = h;
    g_obj_count++;
}

char *gc_new_string_buffer(size_t nbytes) {
    GCHeader *h = calloc(1, sizeof(GCHeader) + nbytes);
    gc_register(h, GC_STRING);
    return (char *)(h + 1);
}

Value gc_protect(Value v) {
    if (g_protect_len >= g_protect_cap) {
        g_protect_cap = g_protect_cap == 0 ? 64 : g_protect_cap * 2;
        g_protect     = realloc(g_protect, g_protect_cap * sizeof(Value));
    }
    g_protect[g_protect_len++] = v;
    return v;
}

void gc_pop(int n) {
    g_protect_len -= n;
    if (g_protect_len < 0) g_protect_len = 0;
}

/* ── Mark ─────────────────────────────────────────────────────────── */

static void gc_mark_map_contents(MorayMap *m) {
    for (int i = 0; i < m->len; i++)
        gc_mark_value(m->pairs[i].value);
}

void gc_mark_value(Value v) {
    switch (v.type) {
        case VAL_STRING: {
            string_header(v.string)->mark = 1;   /* leaf */
            break;
        }
        case VAL_LIST: {
            GCHeader *h = list_header(v.list);
            if (h->mark) return;                  /* already visited (cycles) */
            h->mark = 1;
            for (int i = 0; i < v.list->len; i++)
                gc_mark_value(v.list->data[i]);
            break;
        }
        case VAL_MAP: {
            GCHeader *h = map_header(v.map);
            if (h->mark) return;
            h->mark = 1;
            gc_mark_map_contents(v.map);
            break;
        }
        case VAL_STRUCT: {
            GCHeader *h = struct_header(v.strukt);
            if (h->mark) return;
            h->mark = 1;
            /* The fields map is its own tracked object, reachable only through
               this struct — mark it and its contents so both survive. */
            map_header(v.strukt->fields)->mark = 1;
            gc_mark_map_contents(v.strukt->fields);
            break;
        }
        default:
            break;   /* int/float/bool/null hold no heap memory */
    }
}

/* ── Sweep ────────────────────────────────────────────────────────── */

static void gc_free_object(GCHeader *h) {
    switch ((GCKind)h->kind) {
        case GC_STRING:
            free(h);   /* header is the allocation base for strings */
            break;
        case GC_LIST: {
            MorayList *l = (MorayList *)h;
#ifdef GC_TORTURE
            memset(l->data, 0xDD, (size_t)l->len * sizeof(Value));
#endif
            free(l->data);
            free(l);
            break;
        }
        case GC_MAP: {
            MorayMap *m = (MorayMap *)h;
            for (int i = 0; i < m->len; i++) free(m->pairs[i].key);
            free(m->pairs);
            free(m);
            break;
        }
        case GC_STRUCT: {
            MorayStruct *s = (MorayStruct *)h;
            free(s->type_name);
            /* s->fields is a separately tracked object; it is swept on its own
               pass once it is no longer reachable. */
            free(s);
            break;
        }
    }
    g_obj_count--;
}

void gc_collect(void) {
    /* Mark from every root. */
    env_gc_mark_roots();
    for (int i = 0; i < g_protect_len; i++)
        gc_mark_value(g_protect[i]);

    /* Sweep: free unmarked objects, clear marks on survivors. */
    GCHeader **link = &g_objects;
    while (*link) {
        GCHeader *h = *link;
        if (h->mark) {
            h->mark = 0;
            link    = &h->next;
        } else {
            *link = h->next;       /* unlink before freeing */
            gc_free_object(h);
        }
    }

    /* Grow the threshold so collection cost stays proportional to live data. */
    g_threshold = g_obj_count * 2 < 256 ? 256 : g_obj_count * 2;
}

void gc_maybe_collect(void) {
#ifdef GC_TORTURE
    /* Torture mode: collect at every scope teardown so any missing protection
       surfaces immediately. Enable with -DGC_TORTURE for testing. */
    gc_collect();
#else
    if (g_obj_count >= g_threshold)
        gc_collect();
#endif
}

/* ── String ───────────────────────────────────────────────────────── */

Value val_string(const char *ptr, int len) {
    char *s = gc_new_string_buffer(len + 1);
    memcpy(s, ptr, len);
    s[len] = '\0';
    return (Value){ VAL_STRING, .string = s };
}

/* ── List ─────────────────────────────────────────────────────────── */
/*
 * stdlib equivalent: there is none in standard C.
 * In C++ this would be std::vector<Value>.
 * We're manually managing a growable heap array — same strategy as vec.h
 * but for Value elements stored on the heap so the list can be shared.
 */

Value val_list_empty(void) {
    MorayList *l = calloc(1, sizeof(MorayList));
    gc_register(&l->gc, GC_LIST);
    return (Value){ VAL_LIST, .list = l };
}

void list_push(MorayList *l, Value v) {
    if (l->len >= l->cap) {
        l->cap  = l->cap == 0 ? 8 : l->cap * 2;
        l->data = realloc(l->data, l->cap * sizeof(Value));
    }
    l->data[l->len++] = v;
}

Value list_get(MorayList *l, int index) {
    if (index < 0 || index >= l->len) return val_null();
    return l->data[index];
}

void list_set(MorayList *l, int index, Value v) {
    if (index < 0 || index >= l->len) return;
    value_free(l->data[index]);
    l->data[index] = v;
}

/* ── Map ──────────────────────────────────────────────────────────── */
/*
 * stdlib equivalent: there is none in standard C.
 * In C++ this would be std::unordered_map<std::string, Value>.
 * We're using a simple linear-search array of key/value pairs.
 * Fast enough for small maps; a real hash table would replace this
 * for production use.
 */

/* Allocate a GC-tracked empty map. Shared by val_map_empty and val_struct. */
static MorayMap *gc_new_map(void) {
    MorayMap *m = calloc(1, sizeof(MorayMap));
    gc_register(&m->gc, GC_MAP);
    return m;
}

Value val_map_empty(void) {
    return (Value){ VAL_MAP, .map = gc_new_map() };
}

void map_set(MorayMap *m, const char *key, Value v) {
    /* update existing key if found */
    for (int i = 0; i < m->len; i++) {
        if (strcmp(m->pairs[i].key, key) == 0) {
            value_free(m->pairs[i].value);
            m->pairs[i].value = v;
            return;
        }
    }
    /* insert new key */
    if (m->len >= m->cap) {
        m->cap   = m->cap == 0 ? 8 : m->cap * 2;
        m->pairs = realloc(m->pairs, m->cap * sizeof(MapPair));
    }
    m->pairs[m->len].key   = strdup(key);
    m->pairs[m->len].value = v;
    m->len++;
}

int map_get(MorayMap *m, const char *key, Value *out) {
    for (int i = 0; i < m->len; i++) {
        if (strcmp(m->pairs[i].key, key) == 0) {
            *out = m->pairs[i].value;
            return 1;
        }
    }
    return 0;
}

int map_has(MorayMap *m, const char *key) {
    for (int i = 0; i < m->len; i++)
        if (strcmp(m->pairs[i].key, key) == 0) return 1;
    return 0;
}

/* ── Struct ───────────────────────────────────────────────────────── */
/*
 * A struct instance is a named bag of fields. We reuse the map machinery for
 * the fields, so struct_set/get/has are thin wrappers. Shared by reference.
 */

Value val_struct(const char *type_name) {
    MorayStruct *s = calloc(1, sizeof(MorayStruct));
    gc_register(&s->gc, GC_STRUCT);
    s->type_name   = strdup(type_name);
    s->fields      = gc_new_map();
    return (Value){ VAL_STRUCT, .strukt = s };
}

void struct_set(MorayStruct *s, const char *field, Value v) { map_set(s->fields, field, v); }
int  struct_get(MorayStruct *s, const char *field, Value *out) { return map_get(s->fields, field, out); }
int  struct_has(MorayStruct *s, const char *field) { return map_has(s->fields, field); }

/* ── Shared free / print ──────────────────────────────────────────── */

void value_free(Value v) {
    /*
     * Still a no-op — and now intentionally so for a different reason. Every
     * heap-backed value is shared by handle, so a slot losing its reference
     * (a scope ending, a variable reassigned, a list/map element overwritten)
     * must NOT free the object: other holders may still reach it. Reclamation
     * is the garbage collector's job. Dropping the reference is enough; the next
     * collection (gc_maybe_collect, run when a scope is discarded) frees the
     * object if nothing else can reach it. Kept so the existing "release this
     * slot" call sites remain explicit.
     */
    (void)v;
}

void value_print(Value v) {
    switch (v.type) {
        case VAL_INT:    printf("%ld", v.integer);  break;
        case VAL_FLOAT:  printf("%g",  v.floating); break;
        case VAL_STRING: printf("%s",  v.string);   break;
        case VAL_BOOL:   printf("%s",  v.boolean ? "true" : "false"); break;
        case VAL_NULL:   printf("null"); break;
        case VAL_LIST:
            printf("[");
            for (int i = 0; i < v.list->len; i++) {
                if (i > 0) printf(", ");
                value_print(v.list->data[i]);
            }
            printf("]");
            break;
        case VAL_MAP:
            printf("{");
            for (int i = 0; i < v.map->len; i++) {
                if (i > 0) printf(", ");
                printf("\"%s\": ", v.map->pairs[i].key);
                value_print(v.map->pairs[i].value);
            }
            printf("}");
            break;
        case VAL_STRUCT:
            printf("%s(", v.strukt->type_name);
            for (int i = 0; i < v.strukt->fields->len; i++) {
                if (i > 0) printf(", ");
                printf("%s: ", v.strukt->fields->pairs[i].key);
                value_print(v.strukt->fields->pairs[i].value);
            }
            printf(")");
            break;
    }
}

const char *value_type_name(ValueType t) {
    switch (t) {
        case VAL_INT:    return "int";
        case VAL_FLOAT:  return "float";
        case VAL_STRING: return "string";
        case VAL_BOOL:   return "bool";
        case VAL_NULL:   return "null";
        case VAL_LIST:   return "list";
        case VAL_MAP:    return "map";
        case VAL_STRUCT: return "struct";   /* type() reports the instance's type name instead */
    }
    return "?";
}
