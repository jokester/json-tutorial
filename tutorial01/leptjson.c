#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL */

/**
 * @param c lept_context
 * @param ch char
 */
#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)

/**
 * `context': internal state of a parse
 */
typedef struct {
    const char* json;
}lept_context;

/**
 * skip to next non-space character
 */
static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int lept_parse_null(lept_context* c, lept_value* v) {
    EXPECT(c, 'n');
    if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
        return LEPT_PARSE_INVALID_VALUE;
    /* only modifies state when a "null" is found */
    c->json += 3;
    v->type = LEPT_NULL;
    return LEPT_PARSE_OK;
}

static int lept_parse_true(lept_context* c, lept_value* v) {
    EXPECT(c, 't');
    if (c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = LEPT_TRUE;
    return LEPT_PARSE_OK;
}

static int lept_parse_false(lept_context* c, lept_value* v) {
    EXPECT(c, 'f');
    if (c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's' || c->json[3] != 'e')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 4;
    v->type = LEPT_FALSE;
    return LEPT_PARSE_OK;
}

/**
 */
static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 'n':  return lept_parse_null(c, v);
        case 't':  return lept_parse_true(c, v);
        case 'f':  return lept_parse_false(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
        default:   return LEPT_PARSE_INVALID_VALUE;
    }
}

/**
 * facade: skip whitespace and return first parse result?
 */
int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int parse1;
    assert(v != NULL);
    c.json = json;
    /* FIXME why? */
    v->type = LEPT_NULL;
    /* skip white space */
    lept_parse_whitespace(&c);
    parse1 = lept_parse_value(&c, v);

    if (parse1 != LEPT_PARSE_OK) {
        return parse1;
    }

    /**
     * lept_parse_whitespace again,
     * and throw LEPT_PARSE_ROOT_NOT_SINGULAR if another value is found
     */
    lept_parse_whitespace(&c);
    if (! *c.json) {
        return parse1;
    } else {
        return LEPT_PARSE_ROOT_NOT_SINGULAR;
    }
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}
