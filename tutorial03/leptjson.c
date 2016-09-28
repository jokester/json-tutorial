#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>    /* HUGE_VAL */
#include <stdlib.h>  /* NULL, malloc(), realloc(), free(), strtod() */
#include <string.h>  /* memcpy() */

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch)         do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while(0)

typedef struct {
    const char* json;
    /**
     * point to bottom of stack
     */
    char* stack;
    /**
     * size and top (relative to stack bottom)
     */
    size_t size, top;
}lept_context;

/**
 * increase stack size by `size`
 * @param size: size of new element
 * @return: previous stack top (where new element should be put)
 */
static void* lept_context_push(lept_context* c, size_t size) {
    void* ret;
    assert(c != NULL);
    assert(size > 0);
    /**
     * (re-)alloc the stack if necessary
     */
    if (c->top + size >= c->size) {
        if (c->size == 0)
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size)
            c->size += c->size >> 1;  /* c->size * 1.5 */
        c->stack = (char*)realloc(c->stack, c->size);
        assert(c->stack != NULL);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

/**
 * reduce stack size by `size`
 * @return new stack top (where the poped element resides)
 */
static void* lept_context_pop(lept_context* c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

static void* lept_context_peep(lept_context* c, size_t size) {
    return c->stack + (c->top);
}

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1]; i++)
        if (c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context* c, lept_value* v) {
    const char* p = c->json;
    if (*p == '-') p++;
    if (*p == '0') p++;
    else {
        if (!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
        return LEPT_PARSE_NUMBER_TOO_BIG;
    v->type = LEPT_NUMBER;
    c->json = p;
    return LEPT_PARSE_OK;
}

static int lept_parse_string(lept_context* c, lept_value* v) {
    /**
     * head: */
    size_t head = c->top, len;
    int is_escaping;
    const char* p;
    /* skip first '"' */
    EXPECT(c, '"');
    is_escaping = 0;
    p = c->json;
    for (;;) {
        char ch = *p++;
        if (!is_escaping) {
            switch(ch) {
                case '\"':
                    len = c->top - head;
                    lept_set_string(v, (const char*)lept_context_pop(c, len), len);
                    c->json = p;
                    return LEPT_PARSE_OK;
                case '\0':
                    /** restore stack */
                    c->top = head;
                    return LEPT_PARSE_MISS_QUOTATION_MARK;
                case '\\':
                    is_escaping = 1;
                    continue;
                default:
                    if ( !('\x20' <= ch && ch <= '\x21') && !('\x23' <= ch && ch <= '\x5b') && !('\x5d' <= ch) ) {
                        c->top = head;
                        return LEPT_PARSE_INVALID_STRING_CHAR;
                    }
                    PUTC(c, ch);
            }
        } else {
            is_escaping = 0;
            switch(ch) {
                case '"':
                case '\\':
                case '/':
                    PUTC(c, ch);
                    break;
                case 'b':
                    PUTC(c, '\b');
                    break;
                case 'f':
                    PUTC(c, '\f');
                    break;
                case 'n':
                    PUTC(c, '\n');
                    break;
                case 'r':
                    PUTC(c, '\r');
                    break;
                case 't':
                    PUTC(c, '\t');
                    break;
                default:
                    c->top = head;
                    return LEPT_PARSE_INVALID_STRING_ESCAPE;
            }
        }
    }
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        default:   return lept_parse_number(c, v);
        case '"':  return lept_parse_string(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    lept_init(v);
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

void lept_free(lept_value* v) {
    assert(v != NULL);
    if (v->type == LEPT_STRING)
        free(v->u.s.s);
    v->type = LEPT_NULL;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

int lept_get_boolean(const lept_value* v) {
    assert(v != NULL);
    switch (v->type) {
        case LEPT_TRUE:
            return 1;
        case LEPT_FALSE:
            return 0;
        default:
            assert(0);
            return -1;
    }
}

void lept_set_boolean(lept_value* v, int b) {
    assert(v != NULL);
    assert(b == 0 || b == 1);
    lept_free(v);
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}

void lept_set_number(lept_value* v, double n) {
    assert(v != NULL);
    lept_free(v);
    v->type = LEPT_NUMBER;
    v->u.n = n;
}

const char* lept_get_string(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.s;
}

size_t lept_get_string_length(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}

void lept_set_string(lept_value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    v->u.s.s = (char*)malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = LEPT_STRING;
}
