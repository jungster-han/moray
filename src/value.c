#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "value.h"

Value val_string(const char *ptr, int len) {
    char *s = malloc(len + 1);
    memcpy(s, ptr, len);
    s[len] = '\0';
    return (Value){ VAL_STRING, .string = s };
}

void value_free(Value v) {
    if (v.type == VAL_STRING) free(v.string);
}

void value_print(Value v) {
    switch (v.type) {
        case VAL_INT:    printf("%ld", v.integer);              break;
        case VAL_FLOAT:  printf("%g",  v.floating);             break;
        case VAL_STRING: printf("%s",  v.string);               break;
        case VAL_BOOL:   printf("%s",  v.boolean ? "true" : "false"); break;
        case VAL_NULL:   printf("null");                        break;
    }
}

const char *value_type_name(ValueType t) {
    switch (t) {
        case VAL_INT:    return "int";
        case VAL_FLOAT:  return "float";
        case VAL_STRING: return "string";
        case VAL_BOOL:   return "bool";
        case VAL_NULL:   return "null";
    }
    return "?";
}
