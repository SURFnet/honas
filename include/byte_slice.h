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

#ifndef BYTE_SLICE_H
#define BYTE_SLICE_H

#include "includes.h"
#include "defines.h"

/// \defgroup byte_slice Byte slice operations

/** Byte slice
 *
 * This datastructure represents a sequence of bytes in memory.
 *
 * It's effectively a convenience type for working with uint8_t "pointer + length"
 * and some auxiliary functions one might want to perform on such a chunk of
 * memory.
 *
 * This type is expected to be used "as is" and not through some pointer. One could
 * consider it to effectively be a "fat pointer".
 *
 * In most cases one shouldn't initialize the structure directly but use one of
 * the convenience "constructors":
 *
 * - [byte_slice](@ref byte_slice)`(<some pointer>, <number of bytes>)`
 * - [byte_slice_from_scalar](@ref byte_slice_from_scalar)`(<some scalar type>)`
 * - [byte_slice_from_array](@ref byte_slice_from_array)`(<some scalar array>)`
 * - [byte_slice_from_ptrlen](@ref byte_slice_from_ptrlen)`(<some scalar type>, <number of elements>)`
 *
 * Many of the functions are in the form of inline functions or macros and should in most
 * cases be entirely optimized away by an optimizing C compiler. Especially when the `assert()`
 * checks are disabled by compiling with the NDEBUG define set, of course at the expense of
 * foregoing the bounds and sanity checks.
 */
typedef struct {
	uint8_t* bytes;
	size_t len;
} byte_slice_t;

/** Create a byte slice
 *
 * This is a convenience function to simply create a `byte_slice_t` structure.
 *
 * \param bytes A pointer to the beginning of the sequence of bytes
 * \param len   The number of bytes the sequence is long
 * \returns An initialized `byte_slice_t` structure
 * \ingroup byte_slice
 */
static inline byte_slice_t byte_slice(void* bytes, size_t len)
{
	byte_slice_t slice = { (uint8_t*) bytes, len };
	return slice;
}

/** \def byte_slice_from_scalar
 *
 * This convenience method creates a `byte_slice_t` for a simple scalar
 * value.
 *
 * Example:
 *
 * ```c
 * int number = 13;
 * byte_slice_t number_slice = byte_slice_from_scalar(number);
 * assert(number_slice.len == sizeof(int));
 * ```
 *
 * \param value A scalar value
 * \returns An initialized `byte_slice_t` structure for that scalar value
 * \ingroup byte_slice
 */
#define byte_slice_from_scalar(value) byte_slice((void*) &(value), sizeof(value))

/** \def byte_slice_from_array
 *
 * This convenience method creates a `byte_slice_t` for an array of scalar
 * values.
 *
 * Example:
 *
 * ```c
 * int numbers[4] = { 0 };
 * byte_slice_t numbers_slice = byte_slice_from_array(numbers);
 * assert(numbers_slice.len == sizeof(int) * 4);
 * ```
 *
 * \param value A scalar array
 * \returns An initialized `byte_slice_t` structure for that scalar array
 * \ingroup byte_slice
 */
#define byte_slice_from_array(value) byte_slice((void*) (value), sizeof(value))

/** \def byte_slice_from_array
 *
 * This convenience method creates a `byte_slice_t` for an sequence of scalar
 * values.
 *
 * Example:
 *
 * ```c
 * void func(int *values, size_t nr_values) {
 *   byte_slice_t values_slice = byte_slice_from_ptrlen(values, nr_values);
 *   assert(values_slice.len == sizeof(int) * nr_values);
 * }
 * ```
 *
 * \param value A pointer to the beginning of a sequence of scalar values
 * \param len   The number of scalar values in that sequence
 * \returns An initialized `byte_slice_t` structure for that scalar array
 * \ingroup byte_slice
 */
#define byte_slice_from_ptrlen(value, len) byte_slice((void*) (value), sizeof(*(value)) * len)

/** Set a bit to 1 in the byte slice
 *
 * \param set The byte slice to set the bit in
 * \param idx The index of the bit that should be set to 1
 * \ingroup byte_slice
 */
static inline void byte_slice_set_bit(byte_slice_t slice, size_t bit)
{
	assert(slice.len > (bit >> 3));
	slice.bytes[bit >> 3] |= 1 << (bit & 7);
}

/** Set a number of bits to 1 in the byte slice
 *
 * \param set      The byte slice to set the bit in
 * \param bits     The start of sequence of bit indexes for which the bit should be set to 1
 * \param bits_len The number of bit indexes in the sequence
 * \ingroup byte_slice
 */
static inline void byte_slice_set_bits(byte_slice_t slice, size_t* bits, size_t bits_len)
{
	for (size_t i = 0; i < bits_len; i++)
		byte_slice_set_bit(slice, bits[i]);
}

/** Set a bit to 0 in the byte slice
 *
 * \param set The byte slice to unset the bit in
 * \param idx The index of the bit that should be set to 0
 * \ingroup byte_slice
 */
static inline void byte_slice_unset_bit(byte_slice_t slice, size_t bit)
{
	assert(slice.len > (bit >> 3));
	slice.bytes[bit >> 3] &= ~(1 << (bit & 7));
}

/** Set a number of bits to 0 in the byte slice
 *
 * \param set      The byte slice to set the bit in
 * \param bits     The start of sequence of bit indexes for which the bit should be set to 0
 * \param bits_len The number of bit indexes in the sequence
 * \ingroup byte_slice
 */
static inline void byte_slice_unset_bits(byte_slice_t slice, size_t* bits, size_t bits_len)
{
	for (size_t i = 0; i < bits_len; i++)
		byte_slice_unset_bit(slice, bits[i]);
}

/** Check if bit in the byte slice is set to 1
 *
 * \param set The byte slice to check the bit in
 * \param idx The index of the bit that should be checked
 * \ingroup byte_slice
 */
static inline bool byte_slice_bit_is_set(const byte_slice_t slice, size_t bit)
{
	assert(slice.len > (bit >> 3));
	return (slice.bytes[bit >> 3] & (1 << (bit & 7))) != 0;
}

/** Check if all of a number of bits are set in a byte slice
 *
 * \param set      The byte slice to set the bit in
 * \param bits     The start of sequence of bit indexes for which the bit should be 1
 * \param bits_len The number of bit indexes in the sequence
 * \ingroup byte_slice
 */
static inline bool byte_slice_all_bits_set(const byte_slice_t slice, size_t* bits, size_t bits_len)
{
	for (size_t i = 0; i < bits_len; i++)
		if (!byte_slice_bit_is_set(slice, bits[i]))
			return false;
	return true;
}

/** Check if any of a number of bits are set in a byte slice
 *
 * \param set      The byte slice to set the bit in
 * \param bits     The start of sequence of bit indexes for which the bit should be checked
 * \param bits_len The number of bit indexes in the sequence
 * \ingroup byte_slice
 */
static inline bool byte_slice_any_bit_set(const byte_slice_t slice, size_t* bits, size_t bits_len)
{
	for (size_t i = 0; i < bits_len; i++)
		if (byte_slice_bit_is_set(slice, bits[i]))
			return true;
	return false;
}

/** Set all bytes in a byte slice to 0
 *
 * \param set The byte slice to clear
 * \ingroup byte_slice
 */
static inline void byte_slice_clear(byte_slice_t slice)
{
	memset(slice.bytes, 0, slice.len);
}

/** Determine the number of bits set in the byte slice
 *
 * \param set The byte slice to count the bits in
 * \returns Number of bits set
 * \ingroup byte_slice
 */
extern size_t byte_slice_popcount(const byte_slice_t slice);

/** Bitwise OR two byte slices
 *
 * \param target The byte slice that gets updated
 * \param other  The byte slice who's bits are OR'd with `target`'s bits
 * \ingroup byte_slice
 */
extern void byte_slice_bitwise_or(byte_slice_t target, const byte_slice_t other);

/** Bitwise AND two byte slices
 *
 * \param target The byte slice that gets updated
 * \param other  The byte slice who's bits are AND' with `target`'s bits
 * \ingroup byte_slice
 */
extern void byte_slice_bitwise_and(byte_slice_t target, const byte_slice_t other);

/** Calculate a 64-bit collision resistant hash value of the byte slice using the MurmurHash64A algorithm
 *
 * The hash function has the concept of an additional seed that should be included
 * during the hashing. Correctly providing a randomized seed per item the hash is used
 * for will guard against hash collision attacks.
 *
 * \param slice The byte slice for which the hash should be calculated
 * \param seed  A seed that get's included during the hashing
 * \returns The 64-bit hash value
 * \ingroup byte_slice
 */
static inline uint64_t byte_slice_MurmurHash64A(const byte_slice_t slice, unsigned int seed)
{
	const uint64_t m = 0xc6a4a7935bd1e995;
	const int r = 47;
	uint64_t h = seed ^ (slice.len * m);
	const uint8_t* data = slice.bytes;
	const uint8_t* end = data + (slice.len - (slice.len & 7));

	while (data != end) {
		uint64_t k;
		k = *((uint64_t*)data);
		k *= m;
		k ^= k >> r;
		k *= m;
		h ^= k;
		h *= m;
		data += 8;
	}

	switch(slice.len & 7) {
		case 7: h ^= (uint64_t)data[6] << 48; // fall through
		case 6: h ^= (uint64_t)data[5] << 40; // fall through
		case 5: h ^= (uint64_t)data[4] << 32; // fall through
		case 4: h ^= (uint64_t)data[3] << 24; // fall through
		case 3: h ^= (uint64_t)data[2] << 16; // fall through
		case 2: h ^= (uint64_t)data[1] << 8;  // fall through
		case 1: h ^= (uint64_t)data[0];
			h *= m;
	};

	h ^= h >> r;
	h *= m;
	h ^= h >> r;
	return h;
}

#if defined(USE_ASM)
#if defined(__i386__) || defined(__x86_64__)
#define HAS_BYTE_SLICE_MUL32_ASM
/// \private
static inline uint32_t byte_slice_mul32_asm(byte_slice_t slice, uint32_t multiplier) {
	assert(slice.len % sizeof(uint32_t) == 0);
	uint32_t *data = (uint32_t*) slice.bytes;
	uint32_t *end = data + (slice.len >> 2);
	uint32_t overflow = 0, mul_hi, mul_lo;
	while(data != end) {
		// multiply: 32 bit * 32 bit -> (32 bit, 32 bit)
		__asm__("mull %3"
				: "=d"(mul_hi), "=a"(mul_lo)
				: "a"(multiplier), "rm"(*data)
				: "cc");
		// add: (32 bit, 32 bit) + (0, 32 bit) -> (32 bit, 32 bit)
		__asm__("addl %4, %1\n\tadcl $0, %0"
				: "=r"(mul_hi), "=r"(mul_lo)
				: "0"(mul_hi), "1"(mul_lo), "r"(overflow)
				: "cc");
		*data = mul_lo;
		overflow = mul_hi;
		data++;
	}
	return overflow;
}
#endif /* __i386__ || __x86_64 */
#endif /* USE_ASM */

/// \private
static inline uint32_t byte_slice_mul32_noasm(byte_slice_t slice, uint32_t multiplier) {
	assert(slice.len % sizeof(uint32_t) == 0);
	uint32_t *data = (uint32_t*) slice.bytes;
	uint32_t *end = data + (slice.len >> 2);
	uint32_t overflow = 0, mul_hi, mul_lo;
	while(data != end) {
		uint64_t tmp = ((uint64_t) *data) * ((uint64_t) multiplier);
		mul_lo = tmp;
		mul_hi = tmp >> 32;

		tmp = ((uint64_t) mul_lo) + ((uint64_t) overflow);
		if (tmp > UINT32_MAX) // check for overflow
			mul_hi++;

		*data = tmp;
		overflow = mul_hi;
		data++;
	}
	return overflow;
}

/** Consider the byte slice to be a big integer and multiply it with a 32-bit number in place
 *
 * The multiplication could possible overflow when the multiplied value doesn't fit in the
 * byte slice. The high-order bits that would get lost due to this multiplication are returned.
 *
 * \warning The byte slice must be properly aligned on some platforms
 * \warning The byte slice must be a multiple of `sizeof(uint32_t)` (4 bytes).
 *
 * \param slice      The byte slice that servers as a big integer that get's multiplied
 * \param multiplier The number to multiply the byte slice big integer with
 * \returns          The possible overflow of the calculation
 * \ingroup byte_slice
 */
static inline uint32_t byte_slice_mul32(byte_slice_t slice, uint32_t multiplier) {
#if defined(HAS_BYTE_SLICE_MUL32_ASM)
	return byte_slice_mul32_asm(slice, multiplier);
#else
	return byte_slice_mul32_noasm(slice, multiplier);
#endif
}

#if defined(USE_ASM)
#if defined(__x86_64__)
#define HAS_BYTE_SLICE_MUL64_ASM
/// \private
static inline uint64_t byte_slice_mul64_asm(byte_slice_t slice, uint64_t multiplier) {
	assert(slice.len % sizeof(uint64_t) == 0);
	uint64_t *data = (uint64_t*) slice.bytes;
	uint64_t *end = data + (slice.len >> 3);
	uint64_t overflow = 0, mul_hi, mul_lo;
	while(data != end) {
		// multiply: 64 bit * 64 bit -> (64 bit, 64 bit)
		__asm__("mulq %3"
				: "=d"(mul_hi), "=a"(mul_lo)
				: "a"(multiplier), "rm"(*data)
				: "cc");
		// add: (64 bit, 64 bit) + (0, 64 bit) -> (64 bit, 64 bit)
		__asm__("addq %4, %1\n\tadcq $0, %0"
				: "=r"(mul_hi), "=r"(mul_lo)
				: "0"(mul_hi), "1"(mul_lo), "r"(overflow)
				: "cc");
		*data = mul_lo;
		overflow = mul_hi;
		data++;
	}
	return overflow;
}
#endif /* __x86_64__ */
#endif /* USE_ASM */

#if defined(HAS_128BIT_INTEGERS)
#define HAS_BYTE_SLICE_MUL64_NOASM
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
typedef unsigned __int128 my_uint128_t;
#pragma GCC diagnostic pop

/// \private
static inline uint64_t byte_slice_mul64_noasm(byte_slice_t slice, uint64_t multiplier) {
	assert(slice.len % sizeof(uint64_t) == 0);
	uint64_t *data = (uint64_t*) slice.bytes;
	uint64_t *end = data + (slice.len >> 3);
	uint64_t overflow = 0, mul_hi, mul_lo;
	while(data != end) {
		my_uint128_t tmp = ((my_uint128_t) *data) * ((my_uint128_t) multiplier);
		mul_lo = tmp;
		mul_hi = tmp >> 64;

		tmp = ((my_uint128_t) mul_lo) + ((my_uint128_t) overflow);
		if (tmp > UINT64_MAX) // check for overflow
			mul_hi++;

		*data = tmp;
		overflow = mul_hi;
		data++;
	}
	return overflow;
}
#endif

#if defined(HAS_BYTE_SLICE_MUL64_ASM) || defined(HAS_BYTE_SLICE_MUL64_NOASM)
#define HAS_BYTE_SLICE_MUL64
/** Consider the byte slice to be a big integer and multiply it with a 64-bit number in place
 *
 * The multiplication could possible overflow when the multiplied value doesn't fit in the
 * byte slice. The high-order bits that would get lost due to this multiplication are returned.
 *
 * \warning The byte slice must be properly aligned on some platforms
 * \warning The byte slice must be a multiple of `sizeof(uint64_t)` (8 bytes).
 * \warning The `byte_slice_mul64` is not always available, the `HAS_BYTE_SLICE_MUL64` define
 *          can be used to check for availability at compile time.
 *
 * \param slice      The byte slice that servers as a big integer that get's multiplied
 * \param multiplier The number to multiply the byte slice big integer with
 * \returns          The possible overflow of the calculation
 * \ingroup byte_slice
 */
static inline uint64_t byte_slice_mul64(byte_slice_t slice, uint64_t multiplier) {
#if defined(HAS_BYTE_SLICE_MUL64_ASM)
	return byte_slice_mul64_asm(slice, multiplier);
#else
	return byte_slice_mul64_noasm(slice, multiplier);
#endif
}
#endif

/** Perform a safe cast to a uint32_t pointer
 *
 * The size of the byte slice is checked to be a multiple of `sizeof(uint32_t)`
 * using `assert()`. The slice gets done in way that is conformant with the
 * C strict-aliasing rules.
 *
 * \param slice The byte slice that should be cast
 * \returns The uint32_t pointer pointing at the beginning of the byte slice
 * \ingroup byte_slice
 */
static inline uint32_t *byte_slice_as_uint32_ptr(byte_slice_t slice)
{
	assert(slice.len % sizeof(uint32_t) == 0);
	union {
		uint8_t *bytes;
		uint32_t *ints;
	} aliasing_safe_cast;

	aliasing_safe_cast.bytes = slice.bytes;
	return aliasing_safe_cast.ints;
}

/** Perform a safe cast to a uint64_t pointer
 *
 * The size of the byte slice is checked to be a multiple of `sizeof(uint64_t)`
 * using `assert()`. The slice gets done in way that is conformant with the
 * C strict-aliasing rules.
 *
 * \param slice The byte slice that should be cast
 * \returns The uint64_t pointer pointing at the beginning of the byte slice
 * \ingroup byte_slice
 */
static inline uint64_t *byte_slice_as_uint64_ptr(byte_slice_t slice)
{
	assert(slice.len % sizeof(uint64_t) == 0);
	union {
		uint8_t *bytes;
		uint64_t *ints;
	} aliasing_safe_cast;

	aliasing_safe_cast.bytes = slice.bytes;
	return aliasing_safe_cast.ints;
}

#endif /* BYTE_SLICE_H */
