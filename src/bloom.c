/*
 * Bloom filter - a space-efficient probabilistic data structure
 *
 * For background information, see: https://en.wikipedia.org/wiki/Bloom_filter
 *
 * Based on bloom.c from the qnet-dns-relayd.
 */

/*
 * Copyright (c) 2016-2017, Quarantainenet Holding B.V.
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

#include "bloom.h"
#include "utils.h"
#include "defines.h"

void bloom_determine_offsets(size_t* bit_offsets, size_t bit_offsets_len, size_t filtersize, const byte_slice_t input_hash)
{
	assert(filtersize >= 1 && filtersize < (1 << 29));
	assert(bit_offsets_len >= 1);
	assert(input_hash.len > 0);

	// explicitly initialize bit_offsets
	// (clang-tidy indicated that clang would otherwise think it might be undefined)
	memset(bit_offsets, 0, sizeof(size_t) * bit_offsets_len);

	/* Copy input hash to stack (doesn't clobber 'input_hash' and allows the compiler to see there is no aliasing) */
	uint8_t hash[input_hash.len];
	memcpy(hash, input_hash.bytes, input_hash.len);
	byte_slice_t hash_slice = byte_slice(hash, input_hash.len);

	size_t bs = filtersize << 3; // number of bits in filter
	size_t num_bits = bit_offsets_len > bs ? bs : bit_offsets_len;

#ifdef HAS_BYTE_SLICE_MUL64
	if (hash_slice.len % sizeof(uint64_t) == 0) {
		for (size_t j = num_bits; j > 0; j--) {
			uint64_t overflow = byte_slice_mul64(hash_slice, bs);

			// if we lost some entropy, re-add it
			// gcd(bs, 2 ** (64 * hashsize64)) = 2 ** lost_bits
			// (e.g. if bs is odd, lost_bits is 1)
			assert(sizeof(long int) == sizeof(uint64_t));
			int lost_bits = ffsl(bs);
			if (lost_bits > 1) {
				uint64_t mask = (1 << (lost_bits - 1)) - 1; // lost_bits is in [2..64], bit-shift is ok
				byte_slice_as_uint64_ptr(hash_slice)[0] += overflow & mask;
			}

			// insert new value into bit_offsets[]
			size_t i = j - 1;
			uint32_t _new = overflow;
			while (i + 1 < num_bits && _new >= bit_offsets[i + 1]) {
				bit_offsets[i] = bit_offsets[i + 1];
				i++;
				_new++;
			}
			bit_offsets[i] = _new;
			bs--;
		}
	} else
#endif /* HAS_BYTE_SLICE_MUL64 */
	{
		assert(hash_slice.len % sizeof(uint32_t) == 0);
		for (size_t j = num_bits; j > 0; j--) {
			uint32_t overflow = byte_slice_mul32(hash_slice, bs);

			// if we lost some entropy, re-add it
			// gcd(bs, 2 ** (32 * hashsize)) = 2 ** lost_bits
			// (e.g. if bs is odd, lost_bits is 1)
			assert(sizeof(int) == sizeof(uint32_t));
			int lost_bits = ffs(bs);
			if (lost_bits > 1) {
				uint32_t mask = (1 << (lost_bits - 1)) - 1; // lost_bits is in [2..32], bit-shift is ok
				byte_slice_as_uint32_ptr(hash_slice)[0] += overflow & mask;
			}

			// insert new value into bit_offsets[]
			size_t i = j - 1;
			uint32_t _new = overflow;
			while (i + 1 < num_bits && _new >= bit_offsets[i + 1]) {
				bit_offsets[i] = bit_offsets[i + 1];
				i++;
				_new++;
			}
			bit_offsets[i] = _new;
			bs--;
		}
	}
}

void bloom_set(byte_slice_t filter, const byte_slice_t hash, size_t num_bits)
{
	size_t bit_offsets[num_bits];
	bloom_determine_offsets(bit_offsets, num_bits, filter.len, hash);
	byte_slice_set_bits(filter, bit_offsets, num_bits);
}

bool bloom_is_set(const byte_slice_t filter, const byte_slice_t hash, size_t num_bits)
{
	size_t bit_offsets[num_bits];
	bloom_determine_offsets(bit_offsets, num_bits, filter.len, hash);
	return byte_slice_all_bits_set(filter, bit_offsets, num_bits);
}

size_t bloom_nr_bits_set(const byte_slice_t filter)
{
	return byte_slice_popcount(filter);
}

uint32_t bloom_approx_count(size_t filtersize, size_t num_bits, size_t bits_set)
{
	double m = filtersize << 3;
	double k = num_bits;
	double X = bits_set;
	if (m == X) {
		/* this would otherwise produce 0 due to casting of the double value which should approximate infinity */
		return UINT32_MAX;
	} else {
		/* Based on: https://en.wikipedia.org/wiki/Bloom_filter#Approximating_the_number_of_items_in_a_Bloom_filter */
		return (uint32_t)roundl(-(m / k) * log(1 - (X / m)));
	}
}
