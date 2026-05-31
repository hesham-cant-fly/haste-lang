#include "haste.h"
#include <ctype.h>
#include <stdint.h>
#include <string.h>

struct string string_to_trimed(struct string span)
{
	struct string result = span;

	if (result.len == 0) {
		return result;
	}

	while (result.len > 0 and isspace(result.chars[result.len - 1])) {
		result.len -= 1;
	}

	while (isspace(*result.chars) and *result.chars != '\0' and result.len != 0) {
		result.chars += 1;
		result.len -= 1;
	}

	return result;
}

struct string cstr_as_string(const char *cstr)
{
	uint32_t len = strlen(cstr);
	return string(
		.len = len,
		.chars = cstr);
}

struct string location_as_string(struct location location)
{
	auto source = get_source_file_content(location.len);
	return string(
		.chars = source + location.start,
		.len = location.len);
}

struct string token_as_string(struct token token)
{
	auto source = get_source_file_content(token.src);
	return string(
		.chars = source + token.start,
		.len = token.len);
}

struct location as_location(struct token token)
{
	return location(
		.start = token.start,
		.len = token.len,
		.src = token.src);
}

struct location location_conjoin(
    struct location a,
    struct location b)
{
    uint32_t a_end = a.start + a.len;
    uint32_t b_end = b.start + b.len;
    uint32_t start = a.start < b.start ? a.start : b.start;
    uint32_t end   = a_end  > b_end   ? a_end   : b_end;
    return location(
        .start = start,
        .len   = end - start,
        .src   = a.src);
}
