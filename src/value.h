#ifndef MORAY_VALUE_H
#define MORAY_VALUE_H

#include <stddef.h>

/*
 * A runtime value in Moray.
 *
 * Tagged union: the 'type' field tells you which union member is valid.
 * Reading the wrong member is undefined behavior in C — always check type first.
 */

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL,
    VAL_NULL,
    VAL_LIST,
    VAL_MAP,
    VAL_STRUCT,
} ValueType;

/*
 * Forward declarations. struct Value only ever holds *pointers* to the list,
 * map, and struct instance, so it can be defined before they are. MapPair, on
 * the other hand, embeds a Value by value, so it must come after struct Value
 * is complete.
 */
typedef struct Value       Value;
typedef struct MorayList   MorayList;
typedef struct MorayMap    MorayMap;
typedef struct MorayStruct MorayStruct;

/*
 * Garbage-collection header.
 *
 * Every heap-backed object (string buffer, list, map, struct) carries one of
 * these so the collector can thread it on a single intrusive "all objects" list
 * and mark it during a collection. The header is the FIRST member of each
 * object type, so a GCHeader* and the object pointer are interchangeable; for
 * strings, which have no struct, the header is allocated as a prefix in front of
 * the character data (see gc_new_string_buffer in value.c).
 */
typedef enum { GC_STRING, GC_LIST, GC_MAP, GC_STRUCT } GCKind;

typedef struct GCHeader {
    struct GCHeader *next;   /* next object in the global all-objects list */
    unsigned char    kind;   /* GCKind: how to mark/free this object        */
    unsigned char    mark;   /* set during the mark phase, cleared on sweep */
} GCHeader;

struct Value {
    ValueType type;
    union {
        long          integer;
        double        floating;
        char         *string;    /* heap-allocated, null-terminated */
        int           boolean;   /* 1 or 0 */
        MorayList    *list;      /* heap-allocated, shared */
        MorayMap     *map;       /* heap-allocated, shared */
        MorayStruct  *strukt;    /* heap-allocated, shared ('struct' is a keyword) */
    };
};

/* Heap-allocated list shared by reference */
struct MorayList {
    GCHeader gc;   /* must stay first — see GCHeader */
    Value *data;
    int    len;
    int    cap;
};

/* Heap-allocated map entry */
typedef struct {
    char  *key;    /* heap-allocated string */
    Value  value;
} MapPair;

struct MorayMap {
    GCHeader gc;   /* must stay first — see GCHeader */
    MapPair *pairs;
    int      len;
    int      cap;
};

/*
 * A struct instance. The fields are stored in a map (name -> Value); the type
 * name identifies which struct definition it belongs to, used for method
 * dispatch and type() reporting. Shared by reference, like lists and maps.
 */
struct MorayStruct {
    GCHeader  gc;          /* must stay first — see GCHeader */
    char     *type_name;   /* heap-allocated */
    MorayMap *fields;      /* name -> Value */
};

/* Constructors — one per type so you never set the tag by hand */
static inline Value val_int(long v)         { return (Value){ VAL_INT,    .integer  = v }; }
static inline Value val_float(double v)     { return (Value){ VAL_FLOAT,  .floating = v }; }
static inline Value val_bool(int v)         { return (Value){ VAL_BOOL,   .boolean  = v }; }
static inline Value val_null(void)          { return (Value){ VAL_NULL,   .integer  = 0 }; }
Value val_string(const char *ptr, int len);
Value val_list_empty(void);
Value val_map_empty(void);
Value val_struct(const char *type_name);   /* empty instance, fields added via struct_set */

/* List operations */
void  list_push(MorayList *l, Value v);
Value list_get(MorayList *l, int index);   /* returns null if out of bounds */
void  list_set(MorayList *l, int index, Value v);

/* Map operations */
void  map_set(MorayMap *m, const char *key, Value v);
int   map_get(MorayMap *m, const char *key, Value *out);  /* 1 if found */
int   map_has(MorayMap *m, const char *key);

/* Struct operations (thin wrappers over the field map) */
void  struct_set(MorayStruct *s, const char *field, Value v);
int   struct_get(MorayStruct *s, const char *field, Value *out);  /* 1 if found */
int   struct_has(MorayStruct *s, const char *field);

void        value_free(Value v);
void        value_print(Value v);
const char *value_type_name(ValueType t);

/* ── Garbage collector ────────────────────────────────────────────────
 *
 * Moray reclaims memory with a mark-and-sweep collector. Heap objects are
 * shared by handle (a Value is a shallow copy), so ownership cannot be pinned
 * to any single binding; instead the collector traces every object reachable
 * from a root and frees the rest. A collection is triggered when a scope is
 * discarded (env_free) — i.e. exactly when resources go away.
 *
 * Roots are:
 *   - every live environment's variables (registered by env_new/env_free), and
 *   - the "protect" stack below, which guards values that are in flight on the
 *     C stack and not yet reachable from any environment (function return
 *     values, evaluated call arguments, containers under construction).
 */

/* Allocate a GC-tracked, zeroed char buffer of `nbytes`. Used for every
   VAL_STRING payload so the collector can trace and free strings uniformly. */
char *gc_new_string_buffer(size_t nbytes);

/* Mark one value and everything transitively reachable from it. */
void  gc_mark_value(Value v);

/* Protect stack: pin in-flight temporaries across a possible collection.
   gc_protect returns the value for convenient inline use; gc_pop removes the
   most recently protected `n` entries. Always balance them. */
Value gc_protect(Value v);
void  gc_pop(int n);

/* Run a collection now (sweeps everything unreachable from the roots). */
void  gc_collect(void);

/* Called by env_free when a scope is discarded: collects if enough has been
   allocated since the last collection to make it worthwhile. */
void  gc_maybe_collect(void);

#endif
