#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL, strtod() */
#include <string.h>  /* strlen strncmp */

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)

typedef struct {
    const char* json;
}lept_context;

static int char_whitespace(const char c) {
    return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (char_whitespace(*p))
        p++;
    c->json = p;
}

static int lept_parse_literal(lept_context *c, lept_value *v, const char* token, lept_type type_on_succeed) {
    assert(c);
    assert(v);
    assert(token);

    if (strncmp(c->json, token, strlen(token)) == 0) {
        v->type = type_on_succeed;
        c->json += strlen(token);
        return LEPT_PARSE_OK;
    } else {
        return LEPT_PARSE_INVALID_VALUE;
    }
}

static int lept_parse_true(lept_context* c, lept_value* v) {
    return lept_parse_literal(c, v, "true", LEPT_TRUE);
}

static int lept_parse_false(lept_context* c, lept_value* v) {
    return lept_parse_literal(c, v, "false", LEPT_FALSE);
}

static int lept_parse_null(lept_context* c, lept_value* v) {
    return lept_parse_literal(c, v, "null", LEPT_NULL);
}

static int char_range(const char c, const char left, const char right) {
    return (left <= c) && (c <= right);
}

static int char_range_0_9(const char c) {
    return char_range(c, '0', '9');
}

static int char_range_1_9(const char c) {
    return char_range(c, '1', '9');
}

/**
 * Return true only when c->json starts a valid JSON numeric value
 * 'valid' is defined by following grammer:
 *
 * number = [ "-" ] int [ frac ] [ exp ]
 * int = "0" / digit1-9 *digit
 * frac = "." 1*digit
 * exp = ("e" / "E") ["-" / "+"] 1*digit
 */
static int lept_parse_number_validate(lept_context *c) {
    char* p;
    assert(c);
    assert(c->json);

    p = (char*) c->json;

    /**
     * consumes 0 or 1 '-'
     */
    if (*p == '-') {
        ++p;
    };

    /**
     * remaining should start with [0-9]
     */
    if (!char_range_0_9(*p)) {
        return 0;
    }

    /**
     * consumes regex / 0 | [1-9][0-9]* /
     */
    if (*p == '0') {
        ++p;
    } else if (char_range_1_9(*p)) {
        ++p;
        while(char_range_1_9(*p)) {
            ++p;
        }
    } else {
        return 0;
    }

    /**
     * consumes regex / (\.[0-9]+)? /
     */
    if (*p == '.') {
        char* p2;
        p2 = ++p;

        while (*p && char_range_0_9(*p)) {
            ++p;
        }

        if (p == p2) {
            return 0;
        }
    }

    /**
     * valid if json value ends here
     */
    if (!*p || char_whitespace(*p)) {
        return 1;
    }

    /**
     * consumes a 'e' or 'E'
     */
    if ( (*p == 'e') || (*p == 'E') ) {
        ++p;
    } else {
        return 0;
    }

    if ( (*p == '+') || (*p == '-') ) {
        ++p;
    }

    if ( char_range_1_9(*p)) {
        ++p;
        while(char_range_0_9(*p)) {
            ++p;
        }
    } else {
        return 0;
    }

    if (!*p || char_whitespace(*p) ){
        return 1;
    } else {
        return 0;
    }
}

static int lept_parse_number(lept_context* c, lept_value* v) {
    char* end;
    if (!lept_parse_number_validate(c)) {
        return LEPT_PARSE_INVALID_VALUE;
    }
    v->n = strtod(c->json, &end);
    if (c->json == end)
        return LEPT_PARSE_INVALID_VALUE;
    c->json = end;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't':  return lept_parse_true(c, v);
        case 'f':  return lept_parse_false(c, v);
        case 'n':  return lept_parse_null(c, v);
        default:   return lept_parse_number(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->n;
}
