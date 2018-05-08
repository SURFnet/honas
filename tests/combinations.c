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

#include <check.h>

#define my_assert_uint_array_eq(val, ...)                                                                                                                                                               \
	do {                                                                                                                                                                                                \
		unsigned int _ref[] = { __VA_ARGS__ };                                                                                                                                                          \
		ck_assert_msg(sizeof(val) / sizeof(val[0]) == sizeof(_ref) / sizeof(_ref[0]), "Arrays of different length were used (%u != %u)", sizeof(val) / sizeof(val[0]), sizeof(_ref) / sizeof(_ref[0])); \
		for (size_t i = 0; i < sizeof(_ref) / sizeof(_ref[0]); i++) {                                                                                                                                   \
			if (val[i] != _ref[i]) {                                                                                                                                                                    \
				ck_assert_msg(val[i] == _ref[i], "Arrays don't match at index %d (%u != %u)", i, val[i], _ref[i]);                                                                                      \
			}                                                                                                                                                                                           \
		}                                                                                                                                                                                               \
	} while (0)

START_TEST(test_number_of_combinations)
{
	/* Some simple combination counts */
	ck_assert_uint_eq(number_of_combinations(1, 0), 1);
	ck_assert_uint_eq(number_of_combinations(1, 1), 1);
	ck_assert_uint_eq(number_of_combinations(2, 0), 1);
	ck_assert_uint_eq(number_of_combinations(2, 1), 2);
	ck_assert_uint_eq(number_of_combinations(2, 2), 1);
	ck_assert_uint_eq(number_of_combinations(3, 0), 1);
	ck_assert_uint_eq(number_of_combinations(3, 1), 3);
	ck_assert_uint_eq(number_of_combinations(3, 2), 3);
	ck_assert_uint_eq(number_of_combinations(3, 3), 1);
	ck_assert_uint_eq(number_of_combinations(4, 0), 1);
	ck_assert_uint_eq(number_of_combinations(4, 1), 4);
	ck_assert_uint_eq(number_of_combinations(4, 2), 6);
	ck_assert_uint_eq(number_of_combinations(4, 3), 4);
	ck_assert_uint_eq(number_of_combinations(4, 4), 1);
}
END_TEST

START_TEST(test_lookup_combination_4_2)
{
	/* Check all combinations with set size 4 and subset size 2 */
	uint32_t comb[2];
	lookup_combination(4, comb, 2, 0);
	my_assert_uint_array_eq(comb, 0, 1);
	lookup_combination(4, comb, 2, 1);
	my_assert_uint_array_eq(comb, 0, 2);
	lookup_combination(4, comb, 2, 2);
	my_assert_uint_array_eq(comb, 0, 3);
	lookup_combination(4, comb, 2, 3);
	my_assert_uint_array_eq(comb, 1, 2);
	lookup_combination(4, comb, 2, 4);
	my_assert_uint_array_eq(comb, 1, 3);
	lookup_combination(4, comb, 2, 5);
	my_assert_uint_array_eq(comb, 2, 3);
}
END_TEST

START_TEST(test_lookup_combination_5_3)
{
	/* Check all combinations with set size 4 and subset size 2 */
	uint32_t comb[3];
	lookup_combination(5, comb, 3, 0);
	my_assert_uint_array_eq(comb, 0, 1, 2);
	lookup_combination(5, comb, 3, 1);
	my_assert_uint_array_eq(comb, 0, 1, 3);
	lookup_combination(5, comb, 3, 2);
	my_assert_uint_array_eq(comb, 0, 1, 4);
	lookup_combination(5, comb, 3, 3);
	my_assert_uint_array_eq(comb, 0, 2, 3);
	lookup_combination(5, comb, 3, 4);
	my_assert_uint_array_eq(comb, 0, 2, 4);
	lookup_combination(5, comb, 3, 5);
	my_assert_uint_array_eq(comb, 0, 3, 4);
	lookup_combination(5, comb, 3, 6);
	my_assert_uint_array_eq(comb, 1, 2, 3);
	lookup_combination(5, comb, 3, 7);
	my_assert_uint_array_eq(comb, 1, 2, 4);
	lookup_combination(5, comb, 3, 8);
	my_assert_uint_array_eq(comb, 1, 3, 4);
	lookup_combination(5, comb, 3, 9);
	my_assert_uint_array_eq(comb, 2, 3, 4);
}
END_TEST

Suite* make_suite(void)
{
	TCase* tc_core = tcase_create("Tests");
	tcase_add_test(tc_core, test_number_of_combinations);
	tcase_add_test(tc_core, test_lookup_combination_4_2);
	tcase_add_test(tc_core, test_lookup_combination_5_3);

	Suite* s = suite_create("Combinations");
	suite_add_tcase(s, tc_core);
	return s;
}
