/*
 * Efficient streaming JSON generation
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

#ifndef JSON_PRINTER_H
#define JSON_PRINTER_H

#include "includes.h"

#ifndef JSON_PRINTER_DEBUG
#define JSON_PRINTER_DEBUG 1
#endif

#ifndef JSON_PRINTER_MAX_DEPTH
#if JSON_PRINTER_DEBUG
/* Allow for a greater depth when debugging is enabled, as using 'json_printer_object_key' counts as a level */
#define JSON_PRINTER_MAX_DEPTH 31
#else
#define JSON_PRINTER_MAX_DEPTH 30
#endif
#endif

/** Efficient streaming JSON printer
 *
 * The streaming JSON printer features:
 * - The use of unlocked I/O API while immediately writing the generate JSON
 *   data to the buffered output file. It explicitly locks the file once at
 *   the beginning and unlocks it at the end.
 * - Compile time enabling and disabling of runtime checks that ensure that
 *   the resultant JSON is valid.
 * - No memory (de)allocations being done.
 * - Fully reentrant.
 *
 * \defgroup json_printer Streaming JSON output generation
 */

/** Efficient streaming JSON printer context */
typedef struct {
	FILE* out; ///< \private
	uint32_t depth; ///< \private
	bool first_element; ///< \private
#if JSON_PRINTER_DEBUG
	char stack[JSON_PRINTER_MAX_DEPTH + 1]; /// \private reserve a space for the sentinel value
#endif
} json_printer_t;

/** Start a streaming JSON printer
 *
 * The generated JSON output will be directly written to the file handle. This
 * function must be called before any of the other functions may be called.
 *
 * \note The output file handle will be locked starting from
 *       `json_printer_begin()` until `json_printer_end()` and should not be written
 *       to by any other code during this time.
 *
 * \param ctx The json printer context to be initialized
 * \param out The file handle to write the generated JSON output to
 * \ingroup json_printer
 */
extern void json_printer_begin(json_printer_t* ctx, FILE* out);

/** Stop the streaming JSON printer
 *
 * This function also validates that the generated JSON output is properly
 * terminated. No json printer functions should be called on the supplied json
 * printer context until it's reinitialized again by a call to
 * `json_printer_begin()`.
 *
 * \note This function flushes and unlocks the output file handle that was
 *       locked by `json_printer_begin()`.
 *
 * \param ctx The json printer context to be stopped
 * \ingroup json_printer
 */
extern void json_printer_end(json_printer_t* ctx);


/** Print a json 32-bit unsigned integral number
 *
 * \param ctx The json printer context to use
 * \param val The integer value that should be written
 * \ingroup json_printer
 */
extern void json_printer_uint32(json_printer_t* ctx, uint32_t val);

/** Print a json 64-bit unsigned integral number
 *
 * \param ctx The json printer context to use
 * \param val The integer value that should be written
 * \ingroup json_printer
 */
extern void json_printer_uint64(json_printer_t* ctx, uint64_t val);

/** Print a json boolean value
 *
 * \param ctx The json printer context to use
 * \param val The boolean value that should be written
 * \ingroup json_printer
 */
extern void json_printer_boolean(json_printer_t* ctx, bool val);

/** Print a json string value
 *
 * The string value gets properly escaped according to the JSON specification.
 *
 * \note This function should allow well formatted UTF-8 to pass through,
 *       but performs no further sanity checks on the data.
 *
 * \param ctx The json printer context to use
 * \param val The string value that should be written
 * \ingroup json_printer
 */
extern void json_printer_string(json_printer_t* ctx, const char* val);


/** Print an array open
 *
 * \note This function must be matched by a call to `json_printer_array_end()`.
 *
 * \param ctx The json printer context to use
 * \ingroup json_printer
 */
extern void json_printer_array_begin(json_printer_t* ctx);

/** Print an array close
 *
 * \note This function must be matched by a call to `json_printer_array_begin()`.
 *
 * \param ctx The json printer context to use
 * \ingroup json_printer
 */
extern void json_printer_array_end(json_printer_t* ctx);


/** Print an object open
 *
 * All values printed must be preceded by a call to `json_printer_object_key()`
 * or be done through one of the `json_printer_object_pair_*()` helpers.
 *
 * \note This function must be matched by a call to `json_printer_object_end()`.
 *
 * \param ctx The json printer context to use
 * \ingroup json_printer
 */
extern void json_printer_object_begin(json_printer_t* ctx);

/** Print an object close
 *
 * \note This function must be matched by a call to `json_printer_object_begin()`.
 *
 * \param ctx The json printer context to use
 * \ingroup json_printer
 */
extern void json_printer_object_end(json_printer_t* ctx);

/** Print an object key
 *
 * \note This function must be followed by any of the value printing or array
 *       or object beginnings before a call to `json_printer_object_end()` is made.
 *
 * \param ctx The json printer context to use
 * \param key The object key to print
 * \ingroup json_printer
 */
extern void json_printer_object_key(json_printer_t* ctx, const char* key);


/** Print a json object key with a 32-bit unsigned integral number value
 *
 * This is an optimized helper function that effectively does the same as:
 * - call `json_printer_object_key()`
 * - call `json_printer_uint32()`
 *
 * \param ctx The json printer context to use
 * \param key The object key to print
 * \param val The integer value that should be written
 * \ingroup json_printer
 */
extern void json_printer_object_pair_uint32(json_printer_t* ctx, const char* key, uint32_t val);

/** Print a json object key with a 64-bit unsigned integral number value
 *
 * This is an optimized helper function that effectively does the same as:
 * - call `json_printer_object_key()`
 * - call `json_printer_uint64()`
 *
 * \param ctx The json printer context to use
 * \param key The object key to print
 * \param val The integer value that should be written
 * \ingroup json_printer
 */
extern void json_printer_object_pair_uint64(json_printer_t* ctx, const char* key, uint64_t val);

/** Print a json object key with a json boolean value
 *
 * This is an optimized helper function that effectively does the same as:
 * - call `json_printer_object_key()`
 * - call `json_printer_boolean()`
 *
 * \param ctx The json printer context to use
 * \param key The object key to print
 * \param val The boolean value that should be written
 * \ingroup json_printer
 */
extern void json_printer_object_pair_boolean(json_printer_t* ctx, const char* key, bool val);

/** Print a json object key with a json string value
 *
 * This is an optimized helper function that effectively does the same as:
 * - call `json_printer_object_key()`
 * - call `json_printer_string()`
 *
 * \param ctx The json printer context to use
 * \param key The object key to print
 * \param val The string value that should be written
 * \ingroup json_printer
 */
extern void json_printer_object_pair_string(json_printer_t* ctx, const char* key, const char* val);

#endif /* JSON_PRINTER_H */
