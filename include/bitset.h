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

#ifndef BITSET_H
#define BITSET_H

#include "byte_slice.h"
#include "includes.h"
#include "logging.h"

/// \defgroup bitset Bit set operations

/**
 * A space efficient bitset
 *
 * This is effectively a wrapper around `byte_slice_t` but with an optimization
 * when the bitset is smaller than `byte_slice_t` bits. If a larger bitset is
 * requested than memory will be heap allocated.
 *
 * It performs bound checks on the number of bits using `assert()`.
 *
 * This type externalizes allocation. You declare a variable of type `bitset_t` or
 * include it inside some other data structure. You must call `bitset_create()` before
 * using it and should call `bitset_destroy()` when done.
 *
 * Many of the functions are in the form of inline functions or macros and should in most
 * cases be entirely optimized away by an optimizing C compiler. Especially when the `assert()`
 * checks are disabled by compiling with the NDEBUG define set, of course at the expense of
 * foregoing the bounds and sanity checks.
 */
typedef struct {
	/** Number of bits in bitset */
	size_t num_bits;

	/** The reprentation of the bitset
	 *
	 * If the number of bits is smaller than `byte_slice_t` than don't allocate memory and use the `ints` field.
	 *
	 * Otherwise allocate a chunk of memory from the heap and register that in the `byte_slice` field.
	 *
	 * \private
	 */
	union {
		///! \private
		size_t ints[2];
		///! \private
		byte_slice_t byte_slice;
	} repr;
} bitset_t;

/** Check if the bitset is dynamically allocated in the `byte_slice` field
 *
 * \private
 */
static inline bool bitset_using_byte_slice(bitset_t* set)
{
	return set->num_bits > (sizeof(size_t) << 4);
}

/** Get the `byte_slice_t` representation of this bitset's data
 *
 * \param set The bitset that we want the `byte_slice_t` reprentation of
 * \returns The `byte_slice_t` for the supplied bitset's data
 * \ingroup bitset
 */
static inline byte_slice_t bitset_as_byte_slice(bitset_t* set)
{
	if (bitset_using_byte_slice(set)) {
		return set->repr.byte_slice;
	} else {
		return byte_slice_from_array(set->repr.ints);
	}
}

/** Initialize a bitset
 *
 * This method MUST be called before calling any of the other methods
 * on a bitset. If you want to reuse a `bitset_t` value for a new bitset
 * be sure to first call `bitset_destroy()`.
 *
 * \param set      The bitset to be initialized
 * \param num_bits The number of bits this bitset should represent
 * \ingroup bitset
 */
static inline void bitset_create(bitset_t* set, size_t num_bits)
{
	set->num_bits = num_bits;
	if (bitset_using_byte_slice(set)) {
		set->repr.byte_slice.len = (num_bits + 7) >> 3;
		set->repr.byte_slice.bytes = calloc(set->repr.byte_slice.len, 1);
		log_passert(set->repr.byte_slice.bytes != NULL, "Failed to allocate bitset");
	} else {
		set->repr.ints[0] = 0;
		set->repr.ints[1] = 0;
	}
}

/** Destroy a bitset
 *
 * This method SHOULD be called when a bitset is no longer in use.
 * Failure to do so could lead to memory leaks.
 *
 * \param set The bitset to destroy
 * \ingroup bitset
 */
static inline void bitset_destroy(bitset_t* set)
{
	if (bitset_using_byte_slice(set)) {
		free(set->repr.byte_slice.bytes);
		set->repr.byte_slice.bytes = NULL;
		set->repr.byte_slice.len = 0;
	} else {
		set->repr.ints[0] = 0;
		set->repr.ints[1] = 0;
	}
	set->num_bits = 0;
}

/** Clear all bits in the bitset
 *
 * \param set The bitset to clear all bits in
 * \ingroup bitset
 */
static inline void bitset_clear(bitset_t* set)
{
	byte_slice_clear(bitset_as_byte_slice(set));
}

/** Set a bit to 1 in the bitset
 *
 * \param set The bitset to set the bit in
 * \param idx The index of the bit that should be set to 1
 * \ingroup bitset
 */
static inline void bitset_set_bit(bitset_t* set, size_t idx)
{
	assert(idx < set->num_bits);
	byte_slice_set_bit(bitset_as_byte_slice(set), idx);
}

/** Set a bit to 0 in the bitset
 *
 * \param set The bitset to unset the bit in
 * \param idx The index of the bit that should be set to 0
 * \ingroup bitset
 */
static inline void bitset_unset_bit(bitset_t* set, size_t idx)
{
	assert(idx < set->num_bits);
	byte_slice_unset_bit(bitset_as_byte_slice(set), idx);
}

/** Check if bit in bitset is set to 1
 *
 * \param set The bitset to check the bit in
 * \param idx The index of the bit that should be checked
 * \ingroup bitset
 */
static inline bool bitset_bit_is_set(bitset_t* set, size_t idx)
{
	assert(idx < set->num_bits);
	return byte_slice_bit_is_set(bitset_as_byte_slice(set), idx);
}

/** Bitwise OR two bitsets
 *
 * \param target The bitset that gets updated
 * \param other  The bitset who's bits are OR'd with `target`'s bits
 * \ingroup bitset
 */
static inline void bitset_bitwise_or(bitset_t* target, bitset_t* other)
{
	assert(target->num_bits == other->num_bits);
	byte_slice_bitwise_or(bitset_as_byte_slice(target), bitset_as_byte_slice(other));
}

/** Bitwise AND two bitsets
 *
 * \param target The bitset that gets updated
 * \param other  The bitset who's bits are AND' with `target`'s bits
 * \ingroup bitset
 */
static inline void bitset_bitwise_and(bitset_t* target, bitset_t* other)
{
	assert(target->num_bits == other->num_bits);
	byte_slice_bitwise_and(bitset_as_byte_slice(target), bitset_as_byte_slice(other));
}

/** Determine the number of bits set in the bitset
 *
 * \param set The bitset to count the bits in
 * \returns Number of bits set
 * \ingroup bitset
 */
static inline size_t bitset_popcount(bitset_t* set)
{
	return byte_slice_popcount(bitset_as_byte_slice(set));
}

#endif /* BITSET_H */