#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <math.h>    /* HUGE_VAL */
#include <stdlib.h>  /* NULL, strtod() */
#include <string.h>  /* strlen strncmp */
#include <errno.h>

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         (char_range_0_9(ch))
#define ISDIGIT1TO9(ch)     (char_range_1_9(ch))

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
 *
 * @return
 * 0: not found
 * >= 0: #chars consumed
 */
static int lept_parse_consume_number(lept_context *c) {
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
        return p - c->json;
    }

    /**
     * consumes /[eE][+-]?[0-9]+/
     */
    if ( (*p == 'e') || (*p == 'E') ) {
        ++p;

        if ( (*p == '+') || (*p == '-') ) {
            ++p;
        }

        if (!char_range_0_9(*p)) {
            return 0;
        }

        while(char_range_0_9(*p)) {
            ++p;
        }
    }

    return p - c->json;
}

/**
 * take first (len) chars and convert with strtod
 */
static double strtod_with_len(const char* c, int len, int *error) {
    double result;
    char* temp;
    char* end;

    temp = (char*)malloc(1 + len);
    assert(temp);

    strncpy(temp, c, len);
    *(temp+len) = '\0';

    result = strtod(temp, &end);
    assert(end != temp);
    /* printf("strtod(%s)\n", temp); */
    free(temp);

    /**
     * set error = LEPT_PARSE_NUMBER_TOO_BIG on overflow
     * (ignore underflow)
     */
    if (errno == ERANGE && result != 0) {
        errno = 0;
        *error = LEPT_PARSE_NUMBER_TOO_BIG;
    }

    return result;
}

static int lept_parse_number(lept_context* c, lept_value* v) {
    int chars_in_number;
    int error;
    double number;

    error = 0;
    chars_in_number = lept_parse_consume_number(c);
    /* printf("lept_parse_consume_number(%s) -> %d\n", c->json, chars_in_number); */

    if (!chars_in_number) {
        return LEPT_PARSE_INVALID_VALUE;
    }

    number = strtod_with_len(c->json, chars_in_number, &error);

    if (error) {
        return error;
    }
    c->json += chars_in_number;
    v->n = number;
    v->type = LEPT_NUMBER;

    return LEPT_PARSE_OK;
}

/**
 * Example implementation by milo
 * number = [ "-" ] int [ frac ] [ exp ]
 * int = "0" / digit1-9 *digit
 * frac = "." 1*digit
 * exp = ("e" / "E") ["-" / "+"] 1*digit
 */
static int lept_parse_number2(lept_context* c, lept_value* v) {
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
    v->n = strtod(c->json, NULL);
    if (errno == ERANGE && v->n == HUGE_VAL) return LEPT_PARSE_NUMBER_TOO_BIG;
    v->type = LEPT_NUMBER;
    c->json = p;
    return LEPT_PARSE_OK;
}

/**
 * My implementation - 2
 * number = [ "-" ] int [ frac ] [ exp ]
 * int = "0" / digit1-9 *digit
 * frac = "." 1*digit
 * exp = ("e" / "E") ["-" / "+"] 1*digit
 *
 * regex:
 *
 * / -? ( 0 | [1-9][0-9]* ) (\.[0-9]+)? ( [eE] [-+]? [0-9]+ )? /
 */
static int lept_parse_number3(lept_context* c, lept_value* v) {
    char* p = (char*) c->json;

    if (*p == '-')
        ++p;

    if (*p == '0')
        ++p;
    else if (char_range_1_9(*p)) {
        ++p;
        while (char_range_0_9(*p)) {
            ++p;
        }
    } else {
        return LEPT_PARSE_INVALID_VALUE;
    }

    if (*p == '.') {
        ++p;
        if (!char_range_0_9(*p))
            return LEPT_PARSE_INVALID_VALUE;
        while(char_range_0_9(*p))
            ++p;
    }

    if ( (*p == 'E') || (*p == 'e') ) {
        ++p;

        if ( (*p == '-') || (*p == '+') ) {
            ++p;
        }
        if (!char_range_0_9(*p))
            return LEPT_PARSE_INVALID_VALUE;
        while(char_range_0_9(*p))
            ++p;
    }

    /**
     * error should be reset to 0 before call, and checkeked right after returned from call
     */
    errno = 0;
    v->n = strtod(c->json, NULL);
    if (errno == ERANGE && v->n == HUGE_VAL) return LEPT_PARSE_NUMBER_TOO_BIG;
    v->type = LEPT_NUMBER;
    c->json = p;
    return LEPT_PARSE_OK;

}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't':  return lept_parse_true(c, v);
        case 'f':  return lept_parse_false(c, v);
        case 'n':  return lept_parse_null(c, v);
        default:   return lept_parse_number3(c, v);
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
