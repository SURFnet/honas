/*
 * Bloom filter - a space-efficient probabilistic data structure
 *
 * For background information, see: https://en.wikipedia.org/wiki/Bloom_filter
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

#ifndef BLOOM_H
#define BLOOM_H

#include "byte_slice.h"
#include "includes.h"

/// \defgroup bloom Bloom filter operations

/** This file defines a number of bloomfilter operations
 *
 * The functions operate on a byte slice that represent the bloom filter data.
 *
 * The user is responsible for supplying data using an appropriate hash function, one
 * that should at least have very good collision resistence.
 *
 * E.g. something like [murmurhash3](https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp)
 *
 * Fast hashes which can be used for bloom-filters can be found in the Wikipedia
 * article with a [list of hash functions](https://en.wikipedia.org/wiki/List_of_hash_functions#Non-cryptographic_hash_functions)
 */

/** Add a hashed value to the bloomfilter
 *
 * Add the `hash` of a value to the `filter` where `num_bits` of bits
 * should be set for each unique value.
 *
 * \warning The supplied hash length must be a multiple of `sizeof(uint32_t)`
 *
 * \param filter   The bloom filter to be updated
 * \param hash     The hash of the data that should be added
 * \param num_bits The number of bits that should be set for this item (aka: `k` value)
 * \ingroup bloom
 */
extern void bloom_set(byte_slice_t filter, const byte_slice_t hash, size_t num_bits);

/** Check if hashed value is probably present in the bloom filter
 *
 * Checks if the `hash` of a value is likely present in the `filter`
 * where `num_bits` of bits should be set for each unique value.
 *
 * \warning The supplied hash length must be a multiple of `sizeof(uint32_t)`
 *
 * \param filter   The bloom filter to be updated
 * \param hash     The hash of the data that should be added
 * \param num_bits The number of bits that should be set for this item (aka: `k` value)
 * \returns `true` if the hashed value is probably present, `false` otherwise
 * \ingroup bloom
 */
extern bool bloom_is_set(const byte_slice_t filter, const byte_slice_t hash, size_t num_bits);

/** Determine the number of bits set to 1
 *
 * \param filter The bloom filter to check
 * \returns The number of bits set to 1 in the bloom filter
 * \ingroup bloom
 */
extern size_t bloom_nr_bits_set(const byte_slice_t filter);

/** Determine approximate number of unique values present in a bloom filter
 *
 * \param filtersize The size in bytes of the bloom filter
 * \param num_bits   The number of bits that should be set for this item (aka: `k` value)
 * \param bits_set   The number of bits set to 1 in the bloom filter
 * \returns Approximate number of unique values in the filter.
 * \ingroup bloom
 */
extern uint32_t bloom_approx_count(size_t filtersize, size_t num_bits, size_t bits_set);

/** Determine which bits should be set to 1 in a bloom filter for some hashed value
 *
 * This function is used to determine which offsets should be set for a given hash.
 * (it's mainly exposed for unittest purposes)
 *
 * \param bit_offsets     A sequence of bit indexes that will be updated to indicate which bits should be set
 * \param bit_offsets_len The number of bit indexes that that should be filled (aka: the `k` value of the bloom filter)
 * \param filtersize      The size in bytes of the bloom filter
 * \param input_hash      The hash of the data for which to determine the bit indexes
 * \ingroup bloom
 * \private
 */
extern void bloom_determine_offsets(size_t* bit_offsets, size_t bit_offsets_len, size_t filtersize, const byte_slice_t input_hash);

#endif /* BLOOM_H */
