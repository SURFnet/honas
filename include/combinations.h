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

#ifndef COMBINATIONS_H
#define COMBINATIONS_H

#include "includes.h"

/// \defgroup combinations Reasoning about possible combinations

/** Determine the number of unique combinations of subsets
 *
 * This considers the set to be a sequence of values and the function
 * will determine how many possible distinct indexes in this sequence
 * are possible.
 *
 * \param set_size    The number of elements in the set
 * \param subset_size The number of values in the wanted subsets
 * \returns The number of unique combinations that can be made
 * \ingroup combinations
 */
extern uint32_t number_of_combinations(uint32_t set_size, uint32_t subset_size);

/** Determine the n-th 'combination'
 *
 * Considering a set to be a sequence of values this function will
 * report the indexes of the n-th combination for all possible subsets
 * of indexes into that sequence.
 *
 * \param set_size       The number of elements in the set
 * \param subset_indexes The beginning of a sequence of indexes that will be filled with the indexes for the n-th combination
 * \param subset_size    The number of values in the wanted subset
 * \param combination    The index of the n-th possible combination
 * \ingroup combinations
 */
extern void lookup_combination(uint32_t set_size, uint32_t* subset_indexes, uint32_t subset_size, uint32_t combination);

#endif /* COMBINATIONS_H */
