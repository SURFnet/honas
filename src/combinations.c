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

#include "combinations.h"

/* The official formula is:
 *   factorial(set_size) / factorial(subset_size) / factorial(set_size - subset_size)
 *
 * But this version is faster and more accurate when dealing with larger sizes.
 */
uint32_t number_of_combinations(uint32_t set_size, uint32_t subset_size)
{
	if (subset_size == 0 || subset_size == set_size)
		return 1;
	if (subset_size == 1)
		return set_size;
	return number_of_combinations(set_size - 1, subset_size) + number_of_combinations(set_size - 1, subset_size - 1);
}

void lookup_combination(uint32_t set_size, uint32_t* subset_indexes, uint32_t subset_size, uint32_t combination)
{
	assert(subset_size > 0 && subset_size <= set_size);
	assert(combination < number_of_combinations(set_size, subset_size));

	/* Initialize subset_indexes set */
	for (uint32_t i = 0; i < subset_size; i++)
		subset_indexes[i] = i;

	/* Itterate through combinations until we've reached our target index */
	for (uint32_t c = 0; c < combination; c++) {
		uint32_t i;

		/* Find offset in subset_indexes that is next to be incremented */
		for (i = subset_size - 1; true; i--) {
			if (subset_indexes[i] != i + set_size - subset_size)
				break;
		}
		assert(i != UINT32_MAX); /* We should never run of the start of the set */

		/* Increment offset and update all offsets after that */
		subset_indexes[i]++;
		for (i++; i < subset_size; i++)
			subset_indexes[i] = subset_indexes[i - 1] + 1;
	}
}
