/*
 * Copyright (c) 2017, Quarantainenet Holding B.V.
 * Copyright (c) 2014, Salvatore Sanfilippo <antirez at gmail dot com>
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

#include "hyperloglog.h"

#include <check.h>

static uint64_t uint64_hash(const void* data, size_t data_size)
{
	return byte_slice_MurmurHash64A(byte_slice((void*) data, data_size), 0xadc83b19ULL);
}

START_TEST(test_simple_counts)
{
	hll hll;
	hllInit(&hll);

	const char val_a[] = "foo";
	const char val_b[] = "bar";
	const uint64_t hash_a = uint64_hash(val_a, sizeof(val_a));
	const uint64_t hash_b = uint64_hash(val_b, sizeof(val_b));

	/* Initially the count should be zero */
	ck_assert_int_eq(hllCount(&hll, NULL), 0);

	/* And adding one value should increase that count to one */
	hllAdd(&hll, hash_a);
	ck_assert_int_eq(hllCount(&hll, NULL), 1);

	/* Adding the same value multiple times should not change that count */
	hllAdd(&hll, hash_a);
	hllAdd(&hll, hash_a);
	hllAdd(&hll, hash_a);
	ck_assert_int_eq(hllCount(&hll, NULL), 1);

	/* Adding another value should again increase the count */
	hllAdd(&hll, hash_b);
	ck_assert_int_eq(hllCount(&hll, NULL), 2);

	/* Adding those values multiple times shouldn't further change the count */
	hllAdd(&hll, hash_a);
	hllAdd(&hll, hash_b);
	hllAdd(&hll, hash_b);
	hllAdd(&hll, hash_a);
	ck_assert_int_eq(hllCount(&hll, NULL), 2);

	hllDestroy(&hll);
}
END_TEST

START_TEST(test_sparse_to_dense)
{
	int i;
	hll hll;
	hllInit(&hll);

	ck_assert_int_eq(hll.encoding, HLL_SPARSE);

	for (i = 0; i < 94; i++) {
		hllAdd(&hll, uint64_hash(&i, sizeof(i)));
		ck_assert_int_eq(hll.encoding, HLL_SPARSE);
	}

	/* Should flip to dense */
		hllAdd(&hll, uint64_hash(&i, sizeof(i)));
	ck_assert_int_eq(hll.encoding, HLL_DENSE);

	/* Make sure the cardinality is in the expected range */
	ck_assert_int_ge(hllCount(&hll, NULL), i * 0.9);
	ck_assert_int_le(hllCount(&hll, NULL), i * 1.1);

	hllDestroy(&hll);
}
END_TEST

START_TEST(test_merge_dense_registers)
{
	int i;
	hll hll1;
	hllInit(&hll1);

	/* Build a first hyperloglog with 5000 items */
	for (i = 0; i < 5000; i++)
		hllAdd(&hll1, uint64_hash(&i, sizeof(i)));
	ck_assert_int_eq(hllCount(&hll1, NULL), 5020);
	ck_assert_int_eq(hll1.encoding, HLL_DENSE);

	/* Build a second hyperloglog with 1 different item */
	i++;
	hll hll2;
	hllInit(&hll2);
	hllAdd(&hll2, uint64_hash(&i, sizeof(i)));
	ck_assert_int_eq(hllCount(&hll2, NULL), 1);
	ck_assert_int_eq(hll2.encoding, HLL_SPARSE);

	/* Make sure it's in the "dense" representation */
	hllSparseToDense(&hll2);
	ck_assert_int_eq(hll2.encoding, HLL_DENSE);
	ck_assert_int_eq(hllCount(&hll2, NULL), 1);

	/* Merge the first hyperloglog into the second and check the cardinality */
	byte_slice_bitwise_or(hll2.registers, hll1.registers);
	hll2.card = -1;
	ck_assert_int_eq(hllCount(&hll1, NULL) + 1, hllCount(&hll2, NULL));

	hllDestroy(&hll1);
	hllDestroy(&hll2);
}
END_TEST

START_TEST(test_approximate_count)
{
	hll hll;
	hllInit(&hll);

	/* Test up to 250.000 unique items and see that the counts still
	 * approximately indicate reasonable counts.
	 */
	for (int j = 0; j < 250; j++) {
		int upto = j * 1000;
		for (int i = 0; i < upto; i++)
			hllAdd(&hll, uint64_hash(&i, sizeof(i)));

		int count = hllCount(&hll, NULL);
		ck_assert_int_ge(count, upto * 0.9);
		ck_assert_int_le(count, upto * 1.1);
	}

	hllDestroy(&hll);
}
END_TEST

Suite* make_suite(void)
{
	TCase* tc_core = tcase_create("Tests");
	tcase_add_test(tc_core, test_simple_counts);
	tcase_add_test(tc_core, test_sparse_to_dense);
	tcase_add_test(tc_core, test_approximate_count);
	tcase_add_test(tc_core, test_merge_dense_registers);

	Suite* s = suite_create("Hyperloglog");
	suite_add_tcase(s, tc_core);
	return s;
}
