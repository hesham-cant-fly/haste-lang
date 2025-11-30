#include "converters.h"

static inline int char_to_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return -1;
}

error strn_to_i64(const char *str, size_t n, int base, int64_t *out) {
    if (!str || !out) return ERROR;
    if (base < 2 || base > 36) return ERROR;
    if (n == 0) return ERROR;

    size_t i = 0;

    int negative = 0;
    if (i < n && (str[i] == '+' || str[i] == '-')) {
        negative = (str[i] == '-');
        i += 1;
        if (i == n) return ERROR;
    }

    int64_t value = 0;
    const int64_t max_div = INT64_MAX / base;
    const int64_t max_mod = INT64_MAX % base;

    for (; i < n; i += 1) {
        int d = char_to_digit(str[i]);
        if (d < 0 || d >= base) {
            return ERROR;
        }

        if (value > max_div || (value == max_div && d > max_mod)) {
            if (!negative || (value > max_div || (value == max_div && d > max_mod + 1))) {
                return ERROR;
            }
        }

        value = value * base + d;
    }

    if (negative) {
        value = -value;
    }

    *out = value;
    return OK;
}

error strn_to_double(const char *str, size_t n, double *out) {
    if (!str || !out || n == 0) return ERROR;

    size_t i = 0;

    int negative = 0;
    if (i < n && (str[i] == '+' || str[i] == '-')) {
        negative = (str[i] == '-');
        i += 1;
        if (i == n) {
            return ERROR;
        }
    }

    double int_part = 0.0;
    double frac_part = 0.0;
    double frac_scale = 1.0;

    int saw_digit = 0;

    while (i < n) {
        int d = char_to_digit(str[i]);
        if (d < 0) break;
        saw_digit = 1;

        int_part = int_part * 10.0 + (double)d;
        i += 1;
    }

    if (i < n && str[i] == '.') {
        i += 1;

        while (i < n) {
            int d = char_to_digit(str[i]);
            if (d < 0) break;
            saw_digit = 1;

            frac_scale *= 0.1;
            frac_part += (double)d * frac_scale;
            i += 1;
        }
    }

    if (!saw_digit)
        return ERROR;

    double val = int_part + frac_part;
    if (negative) val = -val;

    *out = val;
    return OK;
}
