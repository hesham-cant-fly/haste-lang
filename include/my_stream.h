/** Created in: 05/25/2026 17:05
  *
  */
#ifndef MY_STREAM_H_
#define MY_STREAM_H_

// #define MY_STREAM_IMPL

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct stream_interface {
	int (*close)(void *data);
	int (*read)(void *data, unsigned char *out_buf, size_t amount);
	int (*write)(void *data, const unsigned char *in, size_t cout);
	int (*seek)(void *data, long int offset, int whence);
	int (*flush)(void *data);
} stream_interface_t;

typedef struct stream {
	void *data;
	const stream_interface_t *vtable;
} stream_t;

typedef struct mem_stream {
	char *buffer;
	size_t len, pos;
} mem_stream_t;

struct modifier_stream {
	const char *current;
	size_t len;
};

// {:[flags][length/base].[precision]w[width]}
struct standard_format_info {
	int has_len;
	int has_base;
	int has_preci;
	int has_width;

	int len;
	int base;
	int preci;
	int width;

	int alternate_form;
	int zero_padded;
};

struct standard_format_info parse_format_info(struct modifier_stream *mod, va_list args);

typedef int (*format_fn_t)(stream_t stream, struct modifier_stream mod, va_list args);

struct format_specifiers {
	size_t len, cap;
	struct format_specifier {
		const char *specifier;
		format_fn_t format;
	} *items;
};

extern struct format_specifiers format_specifiers;
extern stream_t sout;
extern stream_t serr;
extern stream_t stin;

void setup_io_stream(void);
int find_format_specifier(const char *name, struct format_specifier *out);
int define_format_specifier(const char *name, format_fn_t callback);

int print(const char *restrict fmt, ...);
int println(const char *restrict fmt, ...);
int eprint(const char *restrict fmt, ...);
int eprintln(const char *restrict fmt, ...);
int sprint(stream_t stream, const char *restrict fmt, ...);
int sprintln(stream_t stream, const char *restrict fmt, ...);

int snsprint(char *buf, size_t n, const char *restrict fmt, ...);
// int snsprintln(char *buf, size_t n, const char *restrict fmt, ...);

int sputc(stream_t stream, int ch);
int sputs(stream_t stream, const char *restrict s);
int snputs(stream_t stream, const int len, const char *restrict s);

int sread(stream_t stream, unsigned char *out, size_t size, size_t n);
int swrite(stream_t stream, const unsigned char *in, size_t size, size_t n);
int sseek(stream_t stream, long int offset, int whence);
int sflush(stream_t stream);

stream_t smemopen(char *buffer, size_t len);
stream_t sopen(const char *path, const char *mode);
int sclose(stream_t stream);

int vprint(const char *restrict fmt, va_list args);
int vsprint(stream_t stream, const char *restrict fmt, va_list args);
int vsnsprint(char *buf, size_t n, const char *restrict fmt, va_list args);

int has_modifier(const struct modifier_stream *mod);
char peek_modifier(const struct modifier_stream *mod);
char advance_modifier(struct modifier_stream *mod);
int check_modifier(const struct modifier_stream *mod, const char ch);
int match_modifier(struct modifier_stream *mod, const char ch);

# ifdef MY_STREAM_IMPL

static int fs_close(void *data);
static int fs_read(void *data, unsigned char *out_buf, size_t amount);
static int fs_write(void *data, const unsigned char *, size_t);
static int fs_seek(void *data, long int offset, int whence);
static int fs_flush(void *data);

static int mem_close(void *data);
static int mem_read(void *data, unsigned char *out_buf, size_t amount);
static int mem_write_altr(void *data, const unsigned char *in, size_t m); // used for snsprint
static int mem_write(void *data, const unsigned char *, size_t);
static int mem_seek(void *data, long int offset, int whence);
static int mem_flush(void *data);

static const stream_interface_t file_vtable_ = {
	.close = fs_close,
	.read  = fs_read,
	.write = fs_write,
	.seek  = fs_seek,
	.flush = fs_flush,
};

static const stream_interface_t mem_altr_vtable_ = {
	.close = mem_close,
	.read  = mem_read,
	.write = mem_write_altr,
	.seek  = mem_seek,
	.flush = mem_flush,
};

static const stream_interface_t mem_vtable_ = {
	.close = mem_close,
	.read  = mem_read,
	.write = mem_write,
	.seek  = mem_seek,
	.flush = mem_flush,
};

struct format_specifiers format_specifiers = {0};
stream_t sout = {0};
stream_t serr = {0};
stream_t stin = {0};

#  define file_into_stream(...) (stream_t){ .data = (__VA_ARGS__), .vtable = &file_vtable_ }

static int swrite_width(
	stream_t stream,
	const unsigned char *in,
	const int len,
	const int width,
	const char *const padding_char)
{
	int printed_amount = 0;
	for (int i=width - len; 0 < i; i -= 1) {
		printed_amount += swrite(stream, (void*)padding_char, sizeof(char), 1);
	}

	printed_amount += swrite(stream, (void*)in, sizeof(char), len);

	for (int i=width + len; 0 > i; i += 1) {
		printed_amount += swrite(stream, (void*)padding_char, sizeof(char), 1);
	}

	return printed_amount;
}

struct standard_format_info parse_format_info(struct modifier_stream *mod, va_list args)
{
	struct standard_format_info result = {0};
	if (!has_modifier(mod)) return result;
	if (match_modifier(mod, '#')) result.alternate_form = 1;
	if (match_modifier(mod, '0')) result.zero_padded = 1;
	if (match_modifier(mod, '*')) {
		result.has_len = 1;
		result.len = va_arg(args, int);
		result.has_base = 1;
		result.base = result.len;
	} else {
		char *endptr = NULL;
		long m = strtol(mod->current, &endptr, 10);

		if (mod->current != endptr) {
			result.has_len = 1;
			result.len = (int)m;
			result.has_base = 1;
			result.base = result.len;
			mod->current = endptr;
		}
	}
	if (match_modifier(mod, '.')) {
		if (match_modifier(mod, '*')) {
			result.has_preci = 1;
			result.preci = result.width;
		} else {
			char *endptr = NULL;
			const long m = strtol(mod->current, &endptr, 10);

			if (mod->current != endptr) {
				mod->current = endptr;
				result.has_preci = 1;
				result.preci = (int)m;
			}
		}
	}
	if (match_modifier(mod, 'w')) {
		if (match_modifier(mod, '*')) {
			result.has_width = 1;
			result.width = va_arg(args, int);
		} else {
			char *endptr = NULL;
			const long m = strtol(mod->current, &endptr, 10);
			result.has_width = 1;
			result.width = (int)m;
		}
	}

	return result;
}

static int format_char(stream_t stream, struct modifier_stream mod, va_list args)
{
	(void)mod;
	const char ch = va_arg(args, int);
	struct standard_format_info fmt_info = parse_format_info(&mod, args);
	const int width = fmt_info.width;
	const char *const padding_char = fmt_info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)&ch, sizeof(ch), width, padding_char);
}

static int format_lchar(stream_t stream, struct modifier_stream mod, va_list args)
{
	(void)mod;
	const wchar_t ch = va_arg(args, wchar_t);
	struct standard_format_info fmt_info = parse_format_info(&mod, args);
	const int width = fmt_info.width;
	const char *const padding_char = fmt_info.zero_padded ? "0" : " ";

	char buff[5] = {0};
	snprintf(buff, 5, "%lc", ch);
	return swrite_width(stream, (void*)buff, strlen(buff), width, padding_char);
}

static int format_string(stream_t stream, struct modifier_stream mod, va_list args)
{
	const char *str = va_arg(args, const char *);
	if (str == NULL) str = "(nil)";

	struct standard_format_info fmt_info = parse_format_info(&mod, args);
	const int width = fmt_info.width;
	const int input_len = fmt_info.has_len ? fmt_info.len : (int)strlen(str);
	const char *const padding_char = fmt_info.zero_padded ? "0" : " ";
	if (fmt_info.alternate_form) {
		int len = 0;
		len += 1; // for the first "
		for (int i=0; i < input_len; i += 1) {
			const char ch = str[i];
			if (ch == '"'
				|| ch == '\a'
				|| ch == '\b'
				// || ch == '\e'
				|| ch == '\f'
				|| ch == '\n'
				|| ch == '\r'
				|| ch == '\t'
				|| ch == '\v'
				|| ch == '\\'
				|| ch == '\0') {
				len += 1;
			}
			len += 1;
		}
		len += 1; // for the ending "

		// actual printing
		int printed_amount = 0;
		for (int i=width - len; 0 < i; i -= 1) {
			printed_amount += swrite(stream, (void*)padding_char, sizeof(char), 1);
		}

		printed_amount += swrite(stream, (void*)"\"", sizeof(char), 1);
		for (int i=0; i<input_len; i += 1) {
			const char ch = str[i];
			switch (ch) {
			case '"':
				printed_amount += swrite(stream, (void*)"\\\"", sizeof(char), 2);
				break;
			case '\'':
				printed_amount += swrite(stream, (void*)"\\'", sizeof(char), 2);
				break;
			case '\a':
				printed_amount += swrite(stream, (void*)"\\a", sizeof(char), 2);
				break;
			case '\b':
				printed_amount += swrite(stream, (void*)"\\b", sizeof(char), 2);
				break;
			// case '\e':
			// 	printed_amount += swrite(stream, (void*)"\\e", sizeof(char), 2);
			// 	break;
			case '\f':
				printed_amount += swrite(stream, (void*)"\\f", sizeof(char), 2);
				break;
			case '\n':
				printed_amount += swrite(stream, (void*)"\\n", sizeof(char), 2);
				break;
			case '\r':
				printed_amount += swrite(stream, (void*)"\\r", sizeof(char), 2);
				break;
			case '\t':
				printed_amount += swrite(stream, (void*)"\\t", sizeof(char), 2);
				break;
			case '\v':
				printed_amount += swrite(stream, (void*)"\\v", sizeof(char), 2);
				break;
			case '\\':
				printed_amount += swrite(stream, (void*)"\\\\", sizeof(char), 2);
				break;
			case '\0':
				printed_amount += swrite(stream, (void*)"\\0", sizeof(char), 2);
				break;
			default:
				printed_amount += swrite(stream, (void*)&ch, sizeof(char), 1);
				break;
			}
		}
		printed_amount += swrite(stream, (void*)"\"", sizeof(char), 1);

		for (int i=width + len; 0 > i; i += 1) {
			printed_amount += swrite(stream, (void*)padding_char, sizeof(char), 1);
		}

		return printed_amount;
	}

	return swrite_width(stream, (void*)str, input_len, width, padding_char);
}

static char* ll_to_base_str(long long val, char* buf, int alternate, int base)
{
	int upper = 0;
	if (base < 0) {
		upper = 1;
		base *= -1;
	}

    // Basic validation for base
    if (base < 2 || base > 36) {
        buf[0] = '\0';
        return buf;
    }

    char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	if (upper) {
		digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	}
    int i = 0;
    int is_negative = 0;
    unsigned long long u_val = val;

    // Handle 0
    if (val == 0) {
        buf[i++] = '0';
        buf[i] = '\0';
        return buf;
    }

    // Sign handling (only relevant for base 10 usually,
    // but often applied to all signed conversions)
    if (val < 0) {
        is_negative = 1;
        u_val = (unsigned long long)(-val);
    } else {
        u_val = (unsigned long long)val;
    }

    // Conversion logic
    while (u_val > 0) {
        buf[i++] = digits[u_val % base];
        u_val /= base;
    }

    if (is_negative) {
        buf[i++] = '-';
    }

	if (alternate) {
		if (base == 2) {
			buf[i++] = upper ? 'B' : 'b';
			buf[i++] = '0';
		} else if (base == 16) {
			buf[i++] = upper ? 'X' : 'x';
			buf[i++] = '0';
		} else if (base == 8) {
			buf[i++] = '0';
		}
	}

    buf[i] = '\0';

    // Reverse the buffer
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = buf[start];
        buf[start] = buf[end];
        buf[end] = temp;
        start++;
        end--;
    }

    return buf;
}

static char* ull_to_base_str(unsigned long long val, char* buf, int alternate, int base)
{
	int upper = 0;
	if (base < 0) {
		upper = 1;
		base *= -1;
	}

    // Basic validation for base
    if (base < 2 || base > 36) {
        buf[0] = '\0';
        return buf;
    }

    char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	if (upper) {
		digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	}
    int i = 0;
    unsigned long long u_val = val;

    // Handle 0
    if (val == 0) {
        buf[i++] = '0';
        buf[i] = '\0';
        return buf;
    }

    // Conversion logic
    while (u_val > 0) {
        buf[i++] = digits[u_val % base];
        u_val /= base;
    }

	if (alternate) {
		if (base == 2) {
			buf[i++] = upper ? 'B' : 'b';
			buf[i++] = '0';
		} else if (base == 16) {
			buf[i++] = upper ? 'X' : 'x';
			buf[i++] = '0';
		} else if (base == 8) {
			buf[i++] = '0';
		}
	}

    buf[i] = '\0';

    // Reverse the buffer
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = buf[start];
        buf[start] = buf[end];
        buf[end] = temp;
        start++;
        end--;
    }

    return buf;
}

static char* size_to_base_str(size_t val, char* buf, int alternate, int base)
{
	int upper = 0;
	if (base < 0) {
		upper = 1;
		base *= -1;
	}

    // Basic validation for base
    if (base < 2 || base > 36) {
        buf[0] = '\0';
        return buf;
    }

    char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	if (upper) {
		digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	}
    int i = 0;
    size_t u_val = val;

    // Handle 0
    if (val == 0) {
        buf[i++] = '0';
        buf[i] = '\0';
        return buf;
    }

    // Conversion logic
    while (u_val > 0) {
        buf[i++] = digits[u_val % base];
        u_val /= base;
    }

	if (alternate) {
		if (base == 2) {
			buf[i++] = upper ? 'B' : 'b';
			buf[i++] = '0';
		} else if (base == 16) {
			buf[i++] = upper ? 'X' : 'x';
			buf[i++] = '0';
		} else if (base == 8) {
			buf[i++] = '0';
		}
	}

    buf[i] = '\0';

    // Reverse the buffer
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = buf[start];
        buf[start] = buf[end];
        buf[end] = temp;
        start++;
        end--;
    }

    return buf;
}

static char* ptr_to_base_str(void *val, char* buf, int alternate, int base)
{
	int upper = 0;
	if (base < 0) {
		upper = 1;
		base *= -1;
	}

    // Basic validation for base
    if (base < 2 || base > 36) {
        buf[0] = '\0';
        return buf;
    }

    char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	if (upper) {
		digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	}
    int i = 0;
	uintptr_t u_val = (uintptr_t)val;

    // Handle 0
    if (val == 0) {
        buf[i++] = '0';
        buf[i] = '\0';
        return buf;
    }

    // Conversion logic
    while (u_val > 0) {
        buf[i++] = digits[u_val % base];
        u_val /= base;
    }

	if (alternate) {
		if (base == 2) {
			buf[i++] = upper ? 'B' : 'b';
			buf[i++] = '0';
		} else if (base == 16) {
			buf[i++] = upper ? 'X' : 'x';
			buf[i++] = '0';
		} else if (base == 8) {
			buf[i++] = '0';
		}
	}

    buf[i] = '\0';

    // Reverse the buffer
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = buf[start];
        buf[start] = buf[end];
        buf[end] = temp;
        start++;
        end--;
    }

    return buf;
}

static int format_int8(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const int8_t integer = va_arg(args, int);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const int base = info.base ? info.base : 10;
	char buffer[512] = {0};
	ll_to_base_str((long long)integer, buffer, info.alternate_form, base);

	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_int16(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const int16_t integer = va_arg(args, int);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const int base = info.base ? info.base : 10;
	char buffer[512] = {0};
	ll_to_base_str((long long)integer, buffer, info.alternate_form, base);

	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_int32(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const int32_t integer = va_arg(args, int32_t);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const int base = info.base ? info.base : 10;
	char buffer[512] = {0};
	ll_to_base_str((long long)integer, buffer, info.alternate_form, base);

	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_int64(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const int64_t integer = va_arg(args, int64_t);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const int base = info.base ? info.base : 10;
	char buffer[512] = {0};
	ll_to_base_str((long long)integer, buffer, info.alternate_form, base);

	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_uint8(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const uint8_t integer = va_arg(args, unsigned int);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const int base = info.base ? info.base : 10;
	char buffer[512] = {0};
	ll_to_base_str((long long)integer, buffer, info.alternate_form, base);

	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_uint16(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const uint16_t integer = va_arg(args, unsigned int);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const int base = info.base ? info.base : 10;
	char buffer[512] = {0};
	ll_to_base_str((long long)integer, buffer, info.alternate_form, base);

	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_uint32(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const uint32_t integer = va_arg(args, uint32_t);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const int base = info.base ? info.base : 10;
	char buffer[512] = {0};
	ll_to_base_str((long long)integer, buffer, info.alternate_form, base);

	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_uint64(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const uint64_t integer = va_arg(args, uint64_t);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const int base = info.base ? info.base : 10;
	char buffer[512] = {0};
	ll_to_base_str((long long)integer, buffer, info.alternate_form, base);

	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_int(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const int integer = va_arg(args, int);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const int base = info.base ? info.base : 10;
	char buffer[512] = {0};
	ll_to_base_str((long long)integer, buffer, info.alternate_form, base);

	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_l_int(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const long int integer = va_arg(args, long int);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const int base = info.base ? info.base : 10;
	char buffer[512] = {0};
	ll_to_base_str((long long)integer, buffer, info.alternate_form, base);

	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_ll_int(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const long long int integer = va_arg(args, long long int);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const int base = info.base ? info.base : 10;
	char buffer[512] = {0};
	ll_to_base_str((long long)integer, buffer, info.alternate_form, base);

	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_uint(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const unsigned int integer = va_arg(args, unsigned int);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const int base = info.base ? info.base : 10;
	char buffer[512] = {0};
	ull_to_base_str((unsigned long long)integer, buffer, info.alternate_form, base);

	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_l_uint(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const unsigned long int integer = va_arg(args, unsigned long int);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const int base = info.base ? info.base : 10;
	char buffer[512] = {0};
	ull_to_base_str((unsigned long long)integer, buffer, info.alternate_form, base);

	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_ll_uint(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const long long unsigned int integer = va_arg(args, long long unsigned int);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const int base = info.base ? info.base : 10;
	char buffer[512] = {0};
	ull_to_base_str((unsigned long long)integer, buffer, info.alternate_form, base);

	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_size_t(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const size_t integer = va_arg(args, size_t);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const int base = info.base ? info.base : 10;
	char buffer[512] = {0};
	size_to_base_str(integer, buffer, info.alternate_form, base);

	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_ptr(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const void *ptr = va_arg(args, void*);
	const struct standard_format_info info = parse_format_info(&mod, args);
	char buffer[512] = {0};
	ull_to_base_str((unsigned long long)ptr, buffer, 1, 16);
	const char *const padding_char = info.zero_padded ? "0" : " ";
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_float(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const double f = va_arg(args, double);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const char *const padding_char = info.zero_padded ? "0" : " ";
	char buffer[30] = {0};
	snprintf(buffer, sizeof(buffer), "%f", f);
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_double(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const double f = va_arg(args, double);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const char *const padding_char = info.zero_padded ? "0" : " ";
	char buffer[30] = {0};
	snprintf(buffer, sizeof(buffer), "%lf", f);
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

static int format_long_double(
	stream_t stream,
	struct modifier_stream mod,
	va_list args)
{
	const long double f = va_arg(args, long double);
	const struct standard_format_info info = parse_format_info(&mod, args);
	const char *const padding_char = info.zero_padded ? "0" : " ";
	char buffer[30] = {0};
	snprintf(buffer, sizeof(buffer), "%Lf", f);
	return swrite_width(stream, (void*)buffer, strlen(buffer), info.width, padding_char);
}

void setup_io_stream(void)
{
	sout = (stream_t){
		.data = stdout,
		.vtable = &file_vtable_,
	};
	serr = (stream_t){
		.data = stderr,
		.vtable = &file_vtable_,
	};
	stin = (stream_t){
		.data = stdin,
		.vtable = &file_vtable_,
	};

	define_format_specifier("c",   format_char);
	define_format_specifier("lc",  format_lchar);
	define_format_specifier("s",   format_string);

	define_format_specifier("int", format_int);

	define_format_specifier("f", format_float);
	define_format_specifier("lf", format_double);
	define_format_specifier("Lf", format_long_double);

	define_format_specifier("i8", format_int8);
	define_format_specifier("i16", format_int16);
	define_format_specifier("i32", format_int32);
	define_format_specifier("i64", format_int64);

	define_format_specifier("int8", format_int8);
	define_format_specifier("int16", format_int16);
	define_format_specifier("int32", format_int32);
	define_format_specifier("int64", format_int64);

	define_format_specifier("u8", format_uint8);
	define_format_specifier("u16", format_uint16);
	define_format_specifier("u32", format_uint32);
	define_format_specifier("u64", format_uint64);

	define_format_specifier("unt8", format_uint8);
	define_format_specifier("unt16", format_uint16);
	define_format_specifier("unt32", format_uint32);
	define_format_specifier("unt64", format_uint64);

	define_format_specifier("i",   format_int);
	define_format_specifier("li",  format_l_int);
	define_format_specifier("lli", format_ll_int);

	define_format_specifier("u",   format_uint);
	define_format_specifier("lu",  format_l_uint);
	define_format_specifier("llu", format_ll_uint);

	define_format_specifier("d",   format_int);
	define_format_specifier("ld",  format_l_int);
	define_format_specifier("lld", format_ll_int);

	define_format_specifier("z", format_size_t);
	define_format_specifier("usize", format_size_t);

	define_format_specifier("iz", format_size_t);
	define_format_specifier("isize", format_size_t);

	define_format_specifier("p", format_ptr);
	define_format_specifier("ptr", format_ptr);
}

static int is_valid_specifier_name(const char *name)
{
	const char *const banned_characters = "%{} \n\r\t\v\0";
	for (const char *current = name; *current; current += 1) {
		for (const char *ch = banned_characters; *ch; ch += 1) {
			if (*current == *ch) return 0;
		}
	}
	return 1;
}

static int nfind_format_specifier(const char *name, const size_t n, struct format_specifier *out)
{
	for (size_t i=0; i < format_specifiers.len; i++) {
		if (strncmp(name, format_specifiers.items[i].specifier, n) == 0) {
			if (out != NULL) {
				*out = format_specifiers.items[i];
			}
			return 1;
		}
	}
	return 0;
}

int find_format_specifier(const char *name, struct format_specifier *out)
{
	for (size_t i=0; i < format_specifiers.len; i++) {
		if (strcmp(name, format_specifiers.items[i].specifier) == 0) {
			if (out != NULL) {
				*out = format_specifiers.items[i];
			}
			return 1;
		}
	}
	return 0;
}

int define_format_specifier(const char *name, format_fn_t callback)
{
	if (find_format_specifier(name, NULL)) {
		return 0;
	}
	if (format_specifiers.len >= format_specifiers.cap) {
		const size_t new_cap__ = format_specifiers.cap == 0 ? 10 : format_specifiers.cap * 2;
		format_specifiers.items = realloc(format_specifiers.items , (new_cap__ * sizeof(struct format_specifier)));
		format_specifiers.cap = new_cap__ ;
	}
	format_specifiers.items[format_specifiers.len++] = ((struct format_specifier) { .specifier = name , .format = callback,});
	return 1;
}

int print(const char *restrict fmt, ...)
{
	va_list args; va_start(args, fmt);
	int result = vprint(fmt, args);
	va_end(args);
	return result;
}

int println(const char *restrict fmt, ...)
{
	va_list args; va_start(args, fmt);
	int result = vprint(fmt, args);
	va_end(args);
	result += print("\n");
	return result;
}

int eprint(const char *restrict fmt, ...)
{
	va_list args; va_start(args, fmt);
	const int result = vsprint(serr, fmt, args);
	va_end(args);
	return result;
}

int eprintln(const char *restrict fmt, ...)
{
	va_list args; va_start(args, fmt);
	int result = vsprint(serr, fmt, args);
	va_end(args);
	result += sprint(serr, "\n");
	return result;
}

int sprint(stream_t stream, const char *restrict fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int result = vsprint(stream, fmt, args);
	va_end(args);
	return result;
}

int sprintln(stream_t stream, const char *restrict fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int result = vsprint(stream, fmt, args);
	va_end(args);
	result += sprint(stream, "\n");
	return result;
}

int snsprint(char *buf, size_t n, const char *restrict fmt, ...)
{
	va_list args; va_start(args, fmt);
	const int amount = vsnsprint(buf, n, fmt, args);
	va_end(args);
	return amount;
}

// int snsprintln(char *buf, size_t n, const char *restrict fmt, ...)
// {
// 	va_list args; va_start(args, fmt);
// 	int amount = vsnsprint(buf, n, fmt, args);
// 	va_end(args);
// 	amount += println("");
// 	return amount;
// }

int vsnsprint(char *buf, size_t n, const char *restrict fmt, va_list args)
{
	struct mem_stream data = {.buffer = buf, .len = n};
	struct stream strm = (stream_t) { &data, &mem_altr_vtable_ };
	int amount = vsprint(strm, fmt, args);
	amount += sflush(strm);
	return amount;
}

int sputc(stream_t stream, int ch)
{
	unsigned char c = (unsigned char)ch;
	unsigned char chars[] = { c, '\0' };

	int count = swrite(stream, chars, 1, 1);
	if (count != 1) return EOF;

	return (int)c;
}

int sputs(stream_t stream, const char *restrict s)
{
	const size_t len = strlen(s);
	int count = swrite(stream, (const unsigned char*)s, sizeof(char), len);
	if ((size_t)count != len) return EOF;
	return (int)len;
}

int snputs(stream_t stream, const int len, const char *restrict s)
{
	int count = swrite(stream, (const unsigned char*)s, sizeof(char), len);
	if (count != len) return EOF;
	return (int)len;
}

int sread(stream_t stream, unsigned char *out, size_t size, size_t n)
{
	return stream.vtable->read(stream.data, out, size * n);
}

int swrite(stream_t stream, const unsigned char *in, size_t size, size_t n)
{
	return stream.vtable->write(stream.data, in, size * n);
}

int sseek(stream_t stream, long int offset, int whence)
{
	return stream.vtable->seek(stream.data, offset, whence);
}

int sflush(stream_t stream)
{
	return stream.vtable->flush(stream.data);
}

stream_t smemopen(char *buffer, size_t len)
{
	mem_stream_t *data = calloc(sizeof(mem_stream_t), 1);
	data->buffer = buffer;
	data->len = len;
	data->pos = 0;
	return (stream_t) {
		.data = data,
		.vtable = &mem_vtable_,
	};
}

stream_t sopen(const char *path, const char *mode)
{
	FILE *f = fopen(path, mode);
	return file_into_stream(f);
}

int sclose(stream_t stream)
{
	sflush(stream);
	if (stream.vtable != &file_vtable_) {
		free(stream.data);
	}
	return stream.vtable->close(stream.data);
}

int vprint(const char *restrict fmt, va_list args)
{
	return vsprint(sout, fmt, args);
}

int vsprint(stream_t stream, const char *restrict fmt, va_list args)
{
	int printed_amount = 0;

	const char *printing_span = NULL;
	size_t printing_span_len = 0;

	const char *specifier = NULL;
	size_t specifier_len = 0;

	const char *modifier = NULL;
	size_t modifier_len = 0;

	for (const char *current = fmt; *current; current += 1) {
		if (*current == '{') {
			if (printing_span != NULL) {
				printed_amount += swrite(stream, (const unsigned char *)printing_span, sizeof(char), printing_span_len);
			}
			printing_span = NULL;
			printing_span_len = 0;

			current += 1;
			if (*current == '\0') {
				printed_amount += swrite(stream, (void*)"{", sizeof(char), 1);
				break;
			}
			if (*current == '{') {
				printed_amount += swrite(stream, (void*)"{", sizeof(char), 1);
				continue;
			}

			for (specifier = current; *current && *current != ':' && *current != '}'; current += 1) {
				specifier_len += 1;
			}

			if (*current && *current == ':' && current[1] != '}') {
				current += 1;
				for (modifier = current; *current && *current != '}'; current += 1) {
					modifier_len += 1;
				}
			}

			struct format_specifier sp = {0};
			if (nfind_format_specifier(specifier, specifier_len, &sp)) {
				printed_amount += sp.format(stream, (struct modifier_stream) { modifier, modifier_len }, args);
			} else {
				current -= specifier_len;
				current -= modifier_len;
				current -= 1;
				printed_amount += sputc(stream, '{');
			}

			specifier = NULL;
			specifier_len = 0;
			modifier = NULL;
			modifier_len = 0;
			continue;
		}

		if (printing_span_len == 0) {
			printing_span = current;
		}
		printing_span_len += 1;
	}

	if (printing_span != NULL && printing_span_len > 0) {
		printed_amount += swrite(stream, (const unsigned char *)printing_span, sizeof(char), printing_span_len);
	}

	return printed_amount;
}

int has_modifier(const struct modifier_stream *mod)
{
	if (mod->current == NULL) return 0;
	return mod->len != 0;
}

char peek_modifier(const struct modifier_stream *mod)
{
	if (!has_modifier(mod)) {
		return 0x0;
	}

	return mod->current[0];
}

char advance_modifier(struct modifier_stream *mod)
{
	if (!has_modifier(mod)) return 0x0;
	char prev = peek_modifier(mod);

	mod->current += 1;
	mod->len -= 1;

	return prev;
}

int check_modifier(const struct modifier_stream *mod, const char ch)
{
	if (has_modifier(mod)) {
		return peek_modifier(mod) == ch;
	}
	return 0;
}

int match_modifier(struct modifier_stream *mod, const char ch)
{
	if (check_modifier(mod, ch)) {
		advance_modifier(mod);
		return 1;
	}
	return 0;
}

static int fs_close(void *data)
{
	FILE *self = data;
	return fclose(self);
}

static int fs_read(void *data, unsigned char *out_buf, size_t amount)
{
	FILE *self = data;
	return fread(out_buf, 1, amount, self);
}

static int fs_write(void *data, const unsigned char *in, size_t count)
{
	FILE *self = data;
	return fwrite(in, 1, count, self);
}

static int fs_seek(void *data, long int offset, int whence)
{
	FILE *self = data;
	return fseek(self, offset, whence);
}

static int fs_flush(void *data)
{
	FILE *self = data;
	return fflush(self);
}

static int mem_close(void *data)
{
	(void)data;
	return 0;
}

static int mem_read(void *data, unsigned char *out_buf, size_t amount)
{
	(void)data;
	(void)out_buf;
	(void)amount;
	exit(1);
}

static int mem_write_altr(void *data, const unsigned char *in, size_t m)
{
	int amount = mem_write(data, in, m);
	if ((size_t)amount < m) {
		return (int)m;
	}

	return amount;
}

static int mem_write(void *data, const unsigned char *in, size_t m)
{
	mem_stream_t *self = data;

	const size_t remaining = self->len - self->pos;
	if (remaining == 0) return 0;

	const size_t write_amount = m < remaining ? m : remaining - 1;
	memcpy(self->buffer + self->pos, in, sizeof(char) * write_amount);
	self->pos += write_amount;
	self->buffer[self->pos] = '\0';

	return write_amount;
}

static int mem_seek(void *data, long int offset, int whence)
{
	(void)data;
	(void)offset;
	(void)whence;
	exit(1);
}

static int mem_flush(void *data)
{
	(void)data;
	return 0;
}

# endif // MY_STREAM_IMPL

#endif /* !MY_STREAM_H_ */
