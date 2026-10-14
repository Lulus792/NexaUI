#include "nexaui/nexaui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static int parse_valid(const char *text, double expected) {
    double value = 0;
    int valid = 0;
    return nx_numeric_parse(text, strlen(text), -1000, 1000,
                            &value, &valid) == NX_OK && valid &&
           fabs(value - expected) < 1e-12;
}

static int parse_invalid(const char *text) {
    double value = 123;
    int valid = 1;
    return nx_numeric_parse(text, strlen(text), -10, 10,
                            &value, &valid) == NX_OK && !valid && value == 0;
}

static int test_parser(void) {
    CHECK(parse_valid("0", 0));
    CHECK(parse_valid("-0.25e2", -25));
    CHECK(parse_valid(".5", 0.5));
    CHECK(parse_valid("5.", 5));
    CHECK(parse_valid("+3E+2", 300));
    CHECK(parse_valid("00012.50", 12.5));
    CHECK(parse_valid("0.000125", 0.000125));
    CHECK(parse_invalid(""));
    CHECK(parse_invalid("-"));
    CHECK(parse_invalid("."));
    CHECK(parse_invalid("1e"));
    CHECK(parse_invalid("1e+"));
    CHECK(parse_invalid("1,5"));
    CHECK(parse_invalid("NaN"));
    CHECK(parse_invalid(" 2"));
    CHECK(parse_invalid("99"));
    CHECK(parse_invalid("1e9999"));
    CHECK(parse_invalid("1e-9999"));
    double value = 1;
    int valid = 1;
    CHECK(nx_numeric_parse("1\0x", 3, -10, 10, &value, &valid) == NX_OK &&
          !valid && value == 0);
    CHECK(nx_numeric_parse("1", 1, 10, 0, &value, &valid) == NX_ERROR_ARGUMENT);
    return 0;
}

static int expect_draft(nx_text_buffer *buffer, const char *expected) {
    const char *bytes = NULL;
    size_t length = 0;
    return nx_text_buffer_text(buffer, &bytes, &length) == NX_OK &&
           length == strlen(expected) &&
           memcmp(bytes, expected, length) == 0;
}

static int apply(nx_text_buffer *buffer, nx_event event,
                 double *committed, int expected_handled,
                 int expected_valid) {
    int handled = -1, valid = -1;
    return nx_numeric_input_apply_event(buffer, &event, -10, 10,
                                         committed, &handled, &valid) == NX_OK &&
           handled == expected_handled && valid == expected_valid;
}

static int test_editor_bridge(void) {
    nx_text_buffer *buffer = NULL;
    CHECK(nx_text_buffer_create("0", 1, 4096, &buffer) == NX_OK);
    double committed = 0;
    nx_event event = {0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = NX_KEY_A;
    event.modifiers = NX_MOD_CONTROL;
    CHECK(apply(buffer, event, &committed, 1, 1));
    event = (nx_event){0};
    event.type = NX_EVENT_TEXT;
    event.scalar = '-';
    CHECK(apply(buffer, event, &committed, 1, 0));
    CHECK(expect_draft(buffer, "-") && committed == 0);
    event.scalar = '1';
    CHECK(apply(buffer, event, &committed, 1, 1));
    event.scalar = '.';
    CHECK(apply(buffer, event, &committed, 1, 1));
    event.scalar = '5';
    CHECK(apply(buffer, event, &committed, 1, 1));
    CHECK(expect_draft(buffer, "-1.5"));
    event.scalar = 'x';
    CHECK(apply(buffer, event, &committed, 0, 1));
    CHECK(expect_draft(buffer, "-1.5"));
    event.scalar = 0xd800u;
    int handled = -1, valid = -1;
    CHECK(nx_numeric_input_apply_event(buffer, &event, -10, 10,
                                        &committed, &handled, &valid) ==
          NX_ERROR_ARGUMENT && !handled && !valid);
    event.type = NX_EVENT_KEY_DOWN;
    event.key = NX_KEY_ENTER;
    CHECK(apply(buffer, event, &committed, 1, 1));
    CHECK(committed == -1.5 && expect_draft(buffer, "-1.5"));
    event.key = NX_KEY_Z;
    event.modifiers = NX_MOD_CONTROL;
    CHECK(apply(buffer, event, &committed, 1, 1));
    CHECK(expect_draft(buffer, "-1."));
    event.key = NX_KEY_A;
    CHECK(apply(buffer, event, &committed, 1, 1));
    event = (nx_event){0};
    event.type = NX_EVENT_TEXT;
    event.scalar = '9';
    CHECK(apply(buffer, event, &committed, 1, 1));
    CHECK(apply(buffer, event, &committed, 1, 0));
    CHECK(expect_draft(buffer, "99"));
    event.type = NX_EVENT_KEY_DOWN;
    event.key = NX_KEY_ENTER;
    CHECK(apply(buffer, event, &committed, 1, 0));
    CHECK(committed == -1.5);
    event.key = NX_KEY_BACKSPACE;
    CHECK(apply(buffer, event, &committed, 1, 1));
    CHECK(expect_draft(buffer, "9"));
    event.key = NX_KEY_ENTER;
    CHECK(apply(buffer, event, &committed, 1, 1));
    CHECK(committed == 9);
    nx_text_buffer_destroy(buffer);
    return 0;
}

int main(void) {
    if (test_parser()) return 1;
    if (test_editor_bridge()) return 1;
    puts("numeric tests passed");
    return 0;
}
