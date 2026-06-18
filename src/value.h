#ifndef MORAY_VALUE_H
#define MORAY_VALUE_H

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
} ValueType;

typedef struct {
    ValueType type;
    union {
        long        integer;
        double      floating;
        char       *string;   /* heap-allocated, null-terminated */
        int         boolean;  /* 1 or 0 */
    };
} Value;

/* Constructors — one per type so you never set the tag by hand */
static inline Value val_int(long v)         { return (Value){ VAL_INT,    .integer  = v }; }
static inline Value val_float(double v)     { return (Value){ VAL_FLOAT,  .floating = v }; }
static inline Value val_bool(int v)         { return (Value){ VAL_BOOL,   .boolean  = v }; }
static inline Value val_null(void)          { return (Value){ VAL_NULL,   .integer  = 0 }; }
Value val_string(const char *ptr, int len); /* defined in value.c — allocates a copy */

void        value_free(Value v);
void        value_print(Value v);
const char *value_type_name(ValueType t);

#endif
