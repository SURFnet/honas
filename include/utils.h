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

#ifndef UTILS_H
#define UTILS_H

#include "includes.h"

extern int my_strtol(const char* str, int* val);

/** Parse number from a string
 *
 * \param str The string to try and parse a number from
 * \param result A pointer whose value gets updated with the parsed number
 * \param endptr If not NULL than it's updated to point at where the parsing stopped.
 *               If it's NULL than parsing must end at the NUL termination character in `str` for parsing to succeed.
 * \param base The base to use when parsing the number (see `strtoul()` for details about possible `base` values)
 * \returns A booling indicating whether a number has been successfully parsed (and thus if `result` has been updated or not)
 */
extern bool my_strtouint16(const char* str, uint16_t* result, char** endptr, int base);

/** Parse number from a string
 *
 * \param str The string to try and parse a number from
 * \param result A pointer whose value gets updated with the parsed number
 * \param endptr If not NULL than it's updated to point at where the parsing stopped.
 *               If it's NULL than parsing must end at the NUL termination character in `str` for parsing to succeed.
 * \param base The base to use when parsing the number (see `strtoul()` for details about possible `base` values)
 * \returns A booling indicating whether a number has been successfully parsed (and thus if `result` has been updated or not)
 */
extern bool my_strtouint32(const char* str, uint32_t* result, char** endptr, int base);

/** Parse number from a string
 *
 * \param str The string to try and parse a number from
 * \param result A pointer whose value gets updated with the parsed number
 * \param endptr If not NULL than it's updated to point at where the parsing stopped.
 *               If it's NULL than parsing must end at the NUL termination character in `str` for parsing to succeed.
 * \param base The base to use when parsing the number (see `strtoul()` for details about possible `base` values)
 * \returns A booling indicating whether a number has been successfully parsed (and thus if `result` has been updated or not)
 */
extern bool my_strtouint64(const char* str, uint64_t* result, char** endptr, int base);

extern char* create_relative_filepath(const char* orig_file, const char* rel_file);
extern char* index_ws(const char* s);
extern int cmpstringp(const void* p1, const void* p2);

/* Helpful macros for surpressing unused variable/parameter/function warnings
 *
 * Code copied from Stack Overflow (licensed under CC BY-SA 3.0):
 *  https://stackoverflow.com/a/12891181 by ideasman42 (https://stackoverflow.com/users/432509/ideasman42)
 */
#ifdef __GNUC__
/** Used to mark variables as being unused intentionally */
#define UNUSED(x) UNUSED_##x __attribute__((__unused__))
#else
/** Used to mark variables as being intentionally unused */
#define UNUSED(x) UNUSED_##x
#endif

#ifdef __GNUC__
/** Used to mark functions as being unused intentionally */
#define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_##x
#else
/** Used to mark functions as being intentionally unused */
#define UNUSED_FUNCTION(x) UNUSED_##x
#endif

#endif // UTILS_H
