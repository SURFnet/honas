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

#include "byte_slice.h"

#include "defines.h"
#include <popcntintrin.h>

#if defined HAS_BUILTIN_POPCOUNTLL
typedef unsigned long long popcount_t;
#define popcount __builtin_popcountll
#elif defined HAS_BUILTIN_POPCOUNTL
typedef unsigned long popcount_t;
#define popcount __builtin_popcountl
#elif defined HAS_BUILTIN_POPCOUNT
typedef unsigned int popcount_t;
#define popcount __builtin_popcount
#else
/* Fallback implementation of 'popcount'
 *
 * Based on code from Stack Overflow (licensed under CC BY-SA 3.0):
 *  https://stackoverflow.com/a/109025 by Matt Howels (https://stackoverflow.com/users/16881/matt-howells)
 */
typedef uint32_t popcount_t;
static int popcount(popcount_t i)
{
	i = i - ((i >> 1) & 0x55555555);
	i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
	return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}
#endif

#define _opt_align_begin(value, type) ((uint8_t*)((((size_t)value) + sizeof(type) - 1) & ~(sizeof(type) - 1)))
#define _opt_align_end(value, type) ((type*)(((size_t)value) & ~(sizeof(type) - 1)))

size_t byte_slice_popcount(const byte_slice_t slice)
{
	assert(slice.len <= (SIZE_MAX >> 3));
	const uint8_t* end = slice.bytes + slice.len;
	size_t bits = 0;

	union {
		const uint8_t* bytes;
		const popcount_t* opt;
	} cur = { slice.bytes };

	if (slice.len > sizeof(popcount_t) * 2) {
		// Determine the beginning and end of the optimally aligned data
		const uint8_t* opt_begin = _opt_align_begin(slice.bytes, popcount_t);
		const popcount_t* opt_end = _opt_align_end(end, popcount_t);

		// Count bits in bytes until we reach the beginning of
		// optimally aligned data
		while (cur.bytes < opt_begin)
			bits += popcount(*cur.bytes++);

		// Count as many optimally aligned bigger integers as possible
		while (cur.opt < opt_end)
			bits += popcount(*cur.opt++);
	}

	// Count bits in remaining bytes until the end
	while (cur.bytes < end)
		bits += popcount(*cur.bytes++);

	return bits;
}

void byte_slice_bitwise_or(byte_slice_t target, const byte_slice_t other)
{
	assert((size_t) target.bytes % sizeof(size_t) == (size_t) other.bytes % sizeof(size_t));
	size_t len = target.len <= other.len ? target.len : other.len;
	const uint8_t* tgt_end = target.bytes + len;

	union {
		uint8_t* bytes;
		size_t* opt;
	} tgt_cur = { target.bytes };
	union {
		const uint8_t* bytes;
		const size_t* opt;
	} oth_cur = { other.bytes };

	if (len > sizeof(size_t) * 2) {
		// Determine the beginning and end of the optimally aligned data
		const uint8_t* tgt_opt_begin = _opt_align_begin(target.bytes, size_t);
		const size_t* tgt_opt_end = _opt_align_end(tgt_end, size_t);

		// Count bits in bytes until we reach the beginning of
		// optimally aligned data
		while (tgt_cur.bytes < tgt_opt_begin)
			*tgt_cur.bytes++ |= *oth_cur.bytes++;

		// Count as many optimally aligned bigger integers as possible
		while (tgt_cur.opt < tgt_opt_end)
			*tgt_cur.opt++ |= *oth_cur.opt++;
	}

	// Count bits in remaining bytes until the end
	while (tgt_cur.bytes < tgt_end)
		*tgt_cur.bytes++ |= *oth_cur.bytes++;
}

void byte_slice_bitwise_and(byte_slice_t target, const byte_slice_t other)
{
	assert((size_t) target.bytes % sizeof(size_t) == (size_t) other.bytes % sizeof(size_t));
	size_t len = target.len <= other.len ? target.len : other.len;
	const uint8_t* tgt_end = target.bytes + len;

	union {
		uint8_t* bytes;
		size_t* opt;
	} tgt_cur = { target.bytes };
	union {
		const uint8_t* bytes;
		const size_t* opt;
	} oth_cur = { other.bytes };

	if (len > sizeof(size_t) * 2) {
		// Determine the beginning and end of the optimally aligned data
		const uint8_t* tgt_opt_begin = _opt_align_begin(target.bytes, size_t);
		const size_t* tgt_opt_end = _opt_align_end(tgt_end, size_t);

		// Count bits in bytes until we reach the beginning of
		// optimally aligned data
		while (tgt_cur.bytes < tgt_opt_begin)
			*tgt_cur.bytes++ &= *oth_cur.bytes++;

		// Count as many optimally aligned bigger integers as possible
		while (tgt_cur.opt < tgt_opt_end)
			*tgt_cur.opt++ &= *oth_cur.opt++;
	}

	// Count bits in remaining bytes until the end
	while (tgt_cur.bytes < tgt_end)
		*tgt_cur.bytes++ &= *oth_cur.bytes++;
}
