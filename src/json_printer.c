/*
 * Efficient streaming JSON generation
 *
 * Features:
 * - The use of unlocked I/O API while immediately writing the generate JSON
 *   data to the buffered output file. It explicitly locks the file once at
 *   the beginning and unlocks it at the end.
 * - Compile time enabling and disabling of runtime checks that ensure that
 *   the resultant JSON is valid.
 * - No memory (de)allocations being done.
 * - Fully reentrant.
 */

/*
 * Copyright (c) 2017, Quarantainenet Holding B.V.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the company nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "json_printer.h"
#include "logging.h"

#if JSON_PRINTER_DEBUG
#define json_printer_debug_assert(x) assert(x)
#define json_printer_stack_push(ctx, v)                 \
	do {                                                \
		assert((ctx)->depth < JSON_PRINTER_MAX_DEPTH); \
		(ctx)->depth += 1;                              \
		(ctx)->stack[(ctx)->depth] = v;                 \
		(ctx)->first_element = true;                    \
	} while (0)
#define json_printer_stack_push_opt(ctx, v)             \
	do {                                                \
		assert((ctx)->depth < JSON_PRINTER_MAX_DEPTH); \
		(ctx)->depth += 1;                              \
		(ctx)->stack[(ctx)->depth] = v;                 \
		(ctx)->first_element = true;                    \
	} while (0)
#define json_printer_stack_pop(ctx, v)              \
	do {                                            \
		assert((ctx)->stack[(ctx)->depth] == (v));  \
		(ctx)->depth -= 1;                          \
		(ctx)->first_element = ((ctx)->depth == 1); \
	} while (0)
#define json_printer_stack_pop_opt(ctx, v)       \
	do {                                         \
		if ((ctx)->stack[(ctx)->depth] == (v)) { \
			(ctx)->depth -= 1;                   \
		}                                        \
	} while (0)
#define json_printer_stack_assert(ctx, v) \
	assert((ctx)->stack[ctx->depth] == (v))
#define json_printer_stack_assert_expect_value(ctx) \
	do {                                            \
		char sv = (ctx)->stack[(ctx)->depth];       \
		assert(sv == ']' || sv == ':' || sv == 0);  \
	} while (0)
#else
#define json_printer_debug_assert(x) \
	do { /* no-op */                 \
	} while (0)
#define json_printer_stack_push(ctx, v)                 \
	do {                                                \
		assert((ctx)->depth < JSON_PRINTER_MAX_DEPTH); \
		(ctx)->depth += 1;                              \
		(ctx)->first_element = true;                    \
	} while (0)
#define json_printer_stack_push_opt(ctx, v) \
	do {                                    \
		(ctx)->first_element = true;        \
	} while (0)
#define json_printer_stack_pop(ctx, v)              \
	do {                                            \
		assert((ctx)->depth > 0);                   \
		(ctx)->depth -= 1;                          \
		(ctx)->first_element = ((ctx)->depth == 1); \
	} while (0)
#define json_printer_stack_pop_opt(ctx, v) \
	do { /* no-op */                       \
	} while (0)
#define json_printer_stack_assert(ctx, v) \
	do { /* no-op */                      \
	} while (0)
#define json_printer_stack_assert_expect_value(ctx) \
	do { /* no-op */                                \
	} while (0)
#endif

#define json_printer_optional_separator(ctx) \
	do {                                     \
		if (ctx->first_element) {            \
			ctx->first_element = false;      \
		} else {                             \
			putc_unlocked(',', ctx->out);    \
		}                                    \
	} while (0)

static void _json_printer_write_uint32(FILE* out, uint32_t val)
{
	char strbuf[11];
	sprintf(strbuf, "%u", val);
	fputs_unlocked(strbuf, out);
}

static void _json_printer_write_uint64(FILE* out, uint64_t val)
{
	char strbuf[21];
	sprintf(strbuf, "%" PRIu64, val);
	fputs_unlocked(strbuf, out);
}

static void _json_printer_write_boolean(FILE* out, bool val)
{
	fputs_unlocked(val ? "true" : "false", out);
}

static void _json_printer_write_string(FILE* out, const char* val)
{
	uint8_t c;
	putc_unlocked('"', out);
	while (1) {
		c = *val++;
		if (c == 0) {
			/* End of string */
			putc_unlocked('"', out);
			return;
		} else if (c >= 0x20 && c < 0x7e) {
			/* These normal printable characters should be escaped */
			if (c == '"' || c == '\\' || c == '/')
				putc_unlocked('/', out);

			/* Normal printable characters */
			putc_unlocked(c, out);
		} else if (c == '\b') {
			/* Backspace */
			fputs_unlocked("\\b", out);
		} else if (c == '\f') {
			/* Formfeed */
			fputs_unlocked("\\f", out);
		} else if (c == '\n') {
			/* Newline */
			fputs_unlocked("\\n", out);
		} else if (c == '\r') {
			/* Carriage return */
			fputs_unlocked("\\r", out);
		} else if (c == '\t') {
			/* Horizontal tab */
			fputs_unlocked("\\t", out);
		} else if (c > 0x7f) {
			/* Possible UTF-8 byte sequence */
			int len;
			if ((c & 0xe0) == 0xc0)
				len = 2;
			else if ((c & 0xf0) == 0xe0)
				len = 3;
			else if ((c & 0xf8) == 0xf0)
				len = 4;
			else if ((c & 0xfc) == 0xf8)
				len = 5;
			else if ((c & 0xfe) == 0xfc)
				len = 6;
			else
				goto err;

			putc_unlocked(c, out);
			while (--len > 0) {
				c = *val++;
				if (c == 0 || (c & 0xc0) != 0x80)
					goto err;
				putc_unlocked(c, out);
			}
		} else {
			goto err;
		}
	}

err:
	log_die("Encountered invalid character during json string printing: 0x%hhx", c);
}

void json_printer_begin(json_printer_t* ctx, FILE* out)
{
	assert(out != NULL);
	ctx->out = out;
	ctx->depth = 0;
	ctx->first_element = true;
	json_printer_stack_push(ctx, 0); // sentinel value (guards against to many 'json_printer_(object|array)_end()' calls)
	flockfile(ctx->out);
}

void json_printer_end(json_printer_t* ctx)
{
	json_printer_stack_assert(ctx, 0);
	fflush_unlocked(ctx->out);
	funlockfile(ctx->out);
	ctx->out = NULL;
}

void json_printer_uint32(json_printer_t* ctx, uint32_t val)
{
	json_printer_stack_assert_expect_value(ctx);
	json_printer_optional_separator(ctx);
	json_printer_stack_pop_opt(ctx, ':');

	_json_printer_write_uint32(ctx->out, val);
}

void json_printer_uint64(json_printer_t* ctx, uint64_t val)
{
	json_printer_stack_assert_expect_value(ctx);
	json_printer_optional_separator(ctx);
	json_printer_stack_pop_opt(ctx, ':');

	_json_printer_write_uint64(ctx->out, val);
}

void json_printer_boolean(json_printer_t* ctx, bool val)
{
	json_printer_stack_assert_expect_value(ctx);
	json_printer_optional_separator(ctx);
	json_printer_stack_pop_opt(ctx, ':');

	_json_printer_write_boolean(ctx->out, val);
}

void json_printer_string(json_printer_t* ctx, const char* val)
{
	json_printer_stack_assert_expect_value(ctx);
	json_printer_optional_separator(ctx);
	json_printer_stack_pop_opt(ctx, ':');

	_json_printer_write_string(ctx->out, val);
}

void json_printer_array_begin(json_printer_t* ctx)
{
	json_printer_stack_assert_expect_value(ctx);
	json_printer_optional_separator(ctx);
	json_printer_stack_pop_opt(ctx, ':');

	putc_unlocked('[', ctx->out);
	json_printer_stack_push(ctx, ']');
}

void json_printer_array_end(json_printer_t* ctx)
{
	putc_unlocked(']', ctx->out);
	json_printer_stack_pop(ctx, ']');
}

void json_printer_object_begin(json_printer_t* ctx)
{
	json_printer_stack_assert_expect_value(ctx);
	json_printer_optional_separator(ctx);
	json_printer_stack_pop_opt(ctx, ':');

	putc_unlocked('{', ctx->out);
	json_printer_stack_push(ctx, '}');
}

void json_printer_object_key(json_printer_t* ctx, const char* key)
{
	json_printer_stack_assert(ctx, '}');
	json_printer_optional_separator(ctx);
	json_printer_stack_push_opt(ctx, ':');

	_json_printer_write_string(ctx->out, key);
	putc_unlocked(':', ctx->out);
}

void json_printer_object_pair_uint32(json_printer_t* ctx, const char* key, uint32_t val)
{
	json_printer_stack_assert(ctx, '}');
	json_printer_optional_separator(ctx);

	_json_printer_write_string(ctx->out, key);
	putc_unlocked(':', ctx->out);
	_json_printer_write_uint32(ctx->out, val);
}

void json_printer_object_pair_uint64(json_printer_t* ctx, const char* key, uint64_t val)
{
	json_printer_stack_assert(ctx, '}');
	json_printer_optional_separator(ctx);

	_json_printer_write_string(ctx->out, key);
	putc_unlocked(':', ctx->out);
	_json_printer_write_uint64(ctx->out, val);
}

void json_printer_object_pair_boolean(json_printer_t* ctx, const char* key, bool val)
{
	json_printer_stack_assert(ctx, '}');
	json_printer_optional_separator(ctx);

	_json_printer_write_string(ctx->out, key);
	putc_unlocked(':', ctx->out);
	_json_printer_write_boolean(ctx->out, val);
}

void json_printer_object_pair_string(json_printer_t* ctx, const char* key, const char* val)
{
	json_printer_stack_assert(ctx, '}');
	json_printer_optional_separator(ctx);

	_json_printer_write_string(ctx->out, key);
	putc_unlocked(':', ctx->out);
	_json_printer_write_string(ctx->out, val);
}

void json_printer_object_end(json_printer_t* ctx)
{
	putc_unlocked('}', ctx->out);
	json_printer_stack_pop(ctx, '}');
}
