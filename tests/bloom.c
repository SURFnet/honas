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

#include "bloom.h"

#include <check.h>

/* Logic to simplify uint32_t array values assertions */
static char* size_t_array_to_string(size_t* arr, size_t len)
{
	ssize_t strbuflen = 11 * len; /* uint32_t values are max 10 characters and we reserve 1 bytes for the separator or final nul-bytes */
	char* strbuf = malloc(strbuflen + 2); /* allocate 2 bytes extra for '[' and ']' characters */
	char* retval = strbuf;
	*strbuf = '[';
	strbuf++;
	for (size_t i = 0; i < len; i++) {
		int inc = snprintf(strbuf, strbuflen, i == 0 ? "%zu" : ",%zu", arr[i]);
		assert(inc <= strbuflen);
		strbuf += inc;
		strbuflen -= inc;
	}
	*strbuf++ = ']';
	*strbuf = '\0';
	return retval;
}

static char*
size_t_array_check(size_t count, size_t* arr, ...)
{
	size_t ref[count];
	bool mismatch = false;
	va_list refv;
	va_start(refv, arr);
	for (size_t i = 0; i < count; i++) {
		ref[i] = va_arg(refv, uint32_t);
		mismatch = mismatch || ref[i] != arr[i];
	}
	va_end(refv);
	if (mismatch)
		return size_t_array_to_string(ref, count);
	else
		return NULL;
}

#define ck_assert_size_t_array_eq(len, arr, ...)                                           \
	{                                                                                      \
		char* mismatched_other = size_t_array_check(len, arr, __VA_ARGS__);                \
		(mismatched_other)                                                                 \
			? ck_abort_msg("%s != %s", size_t_array_to_string(arr, len), mismatched_other) \
			: _mark_point(__FILE__, __LINE__);                                             \
	}

/* Some helpers to call the bloom functions with only a single uint32_t as hash value */
static void bloom_set_single(byte_slice_t filter, uint32_t value, uint32_t num_bits)
{
	bloom_set(filter, byte_slice_from_scalar(value), num_bits);
}

static bool bloom_is_set_single(byte_slice_t filter, uint32_t value, uint32_t num_bits)
{
	return bloom_is_set(filter, byte_slice_from_scalar(value), num_bits);
}

static void bloom_determine_offsets_single(size_t* bit_offsets, size_t num_bits, size_t filtersize, uint32_t value)
{
	bloom_determine_offsets(bit_offsets, num_bits, filtersize, byte_slice_from_scalar(value));
}

START_TEST(test_bloom_offsets)
{
	size_t bit_offsets[6] = { 0 };
	ck_assert_size_t_array_eq(6, bit_offsets, 0, 0, 0, 0, 0, 0);

	/* Check some variations of a misc hash value */
	bloom_determine_offsets_single(bit_offsets, 2, 1, 0xdeadbeef);
	ck_assert_size_t_array_eq(2, bit_offsets, 6, 7);

	bloom_determine_offsets_single(bit_offsets, 2, 1024, 0xdeadbeef);
	ck_assert_size_t_array_eq(2, bit_offsets, 5883, 7125);

	bloom_determine_offsets_single(bit_offsets, 3, 1024, 0xdeadbeef);
	ck_assert_size_t_array_eq(3, bit_offsets, 243, 5883, 7125);

	bloom_determine_offsets_single(bit_offsets, 2, 8192, 0xdeadbeef);
	ck_assert_size_t_array_eq(2, bit_offsets, 48879, 57005);

	/* Similar variations of another misc hash value are completely different */
	bloom_determine_offsets_single(bit_offsets, 2, 1, 0x99c0ffee);
	ck_assert_size_t_array_eq(2, bit_offsets, 4, 6);

	bloom_determine_offsets_single(bit_offsets, 2, 1024, 0x99c0ffee);
	ck_assert_size_t_array_eq(2, bit_offsets, 1023, 4920);

	bloom_determine_offsets_single(bit_offsets, 6, 1024, 0x99c0ffee);
	ck_assert_size_t_array_eq(6, bit_offsets, 79, 1023, 1910, 4920, 4941, 7705);

	bloom_determine_offsets_single(bit_offsets, 2, 8192, 0x99c0ffee);
	ck_assert_size_t_array_eq(2, bit_offsets, 39360, 65518);

	/* Even very similar values should start diverging in bit offsets */
	bloom_determine_offsets_single(bit_offsets, 3, 1024, 10);
	ck_assert_size_t_array_eq(3, bit_offsets, 0, 1, 1281);

	bloom_determine_offsets_single(bit_offsets, 3, 1024, 42);
	ck_assert_size_t_array_eq(3, bit_offsets, 0, 1, 5376);

	/* Some other misc values */
	bloom_determine_offsets_single(bit_offsets, 3, 1024, 0);
	ck_assert_size_t_array_eq(3, bit_offsets, 0, 1, 2);

	bloom_determine_offsets_single(bit_offsets, 3, 1024, 0xffffffff);
	ck_assert_size_t_array_eq(3, bit_offsets, 8189, 8190, 8191);
}
END_TEST

START_TEST(test_filter_basics)
{
	uint8_t filter_data[8192] = { 0 };
	byte_slice_t filter = byte_slice_from_array(filter_data);
	ck_assert_int_eq(bloom_nr_bits_set(filter), 0);

	/* Value should not be present in newly created filter */
	ck_assert(!bloom_is_set_single(filter, 0xdeadbeef, 2));
	ck_assert(!bloom_is_set_single(filter, 0x99c0ffee, 2));

	/* Adding the value should mark it present */
	bloom_set_single(filter, 0xdeadbeef, 2);
	ck_assert(bloom_is_set_single(filter, 0xdeadbeef, 2));
	ck_assert(!bloom_is_set_single(filter, 0x99c0ffee, 2));
	ck_assert_int_eq(bloom_nr_bits_set(filter), 2);

	/* Adding the same value again should not change anything */
	bloom_set_single(filter, 0xdeadbeef, 2);
	ck_assert(bloom_is_set_single(filter, 0xdeadbeef, 2));
	ck_assert(!bloom_is_set_single(filter, 0x99c0ffee, 2));
	ck_assert_int_eq(bloom_nr_bits_set(filter), 2);

	/* Adding the other value should not influence the first one and the new one should be present */
	bloom_set_single(filter, 0x99c0ffee, 2);
	ck_assert(bloom_is_set_single(filter, 0xdeadbeef, 2));
	ck_assert(bloom_is_set_single(filter, 0x99c0ffee, 2));
	ck_assert_int_eq(bloom_nr_bits_set(filter), 4);

	/* Adding the same value again should not change anything */
	bloom_set_single(filter, 0x99c0ffee, 2);
	ck_assert(bloom_is_set_single(filter, 0xdeadbeef, 2));
	ck_assert(bloom_is_set_single(filter, 0x99c0ffee, 2));
	ck_assert_int_eq(bloom_nr_bits_set(filter), 4);
}
END_TEST

START_TEST(test_filter_basics_with_overlap)
{
	uint8_t filter_data[1] = { 0 }; /* we use an insanely small bloom filter of just 8-bits */
	byte_slice_t filter = byte_slice_from_array(filter_data);
	ck_assert_int_eq(bloom_nr_bits_set(filter), 0);

	/* Value should not be present in newly created filter */
	ck_assert(!bloom_is_set_single(filter, 0xdeadbeef, 2));
	ck_assert(!bloom_is_set_single(filter, 0x99c0ffee, 2));

	/* Adding the value should mark it present */
	bloom_set_single(filter, 0xdeadbeef, 2);
	ck_assert(bloom_is_set_single(filter, 0xdeadbeef, 2));
	ck_assert(!bloom_is_set_single(filter, 0x99c0ffee, 2));
	ck_assert_int_eq(bloom_nr_bits_set(filter), 2);

	/* Adding the same value again should not change anything */
	bloom_set_single(filter, 0xdeadbeef, 2);
	ck_assert(bloom_is_set_single(filter, 0xdeadbeef, 2));
	ck_assert(!bloom_is_set_single(filter, 0x99c0ffee, 2));
	ck_assert_int_eq(bloom_nr_bits_set(filter), 2);

	/* Adding the other value should not influence the first one and the new one should be present */
	bloom_set_single(filter, 0x99c0ffee, 2);
	ck_assert(bloom_is_set_single(filter, 0xdeadbeef, 2));
	ck_assert(bloom_is_set_single(filter, 0x99c0ffee, 2));
	ck_assert_int_eq(bloom_nr_bits_set(filter), 3);

	/* Adding the same value again should not change anything */
	bloom_set_single(filter, 0x99c0ffee, 2);
	ck_assert(bloom_is_set_single(filter, 0xdeadbeef, 2));
	ck_assert(bloom_is_set_single(filter, 0x99c0ffee, 2));
	ck_assert_int_eq(bloom_nr_bits_set(filter), 3);
}
END_TEST

START_TEST(test_filter_fill)
{
	uint8_t filter_data[1024] = { 0 };
	byte_slice_t filter = byte_slice_from_array(filter_data);
	ck_assert_int_eq(bloom_nr_bits_set(filter), 0);

	/* These values were manually picked and have a high change of collisions */
	uint32_t values[10] = { 0, 10, 42, 1337, 65535, 65536, 1213141516, 0xdeadbeef, 0x99c0ffee, 0xffffffff };
	for (size_t i = 0; i < 10; i++) {
		ck_assert(!bloom_is_set_single(filter, values[i], 3));
		bloom_set_single(filter, values[i], 3);
		ck_assert(bloom_is_set_single(filter, values[i], 3));
	}

	/* Due to the nature of the picked numbers the number of bits and approximate counts are way off */
	ck_assert_uint_eq(bloom_nr_bits_set(filter), 22);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 22), 7);
}
END_TEST

START_TEST(test_filter_fill_random)
{
	uint8_t filter_data[1024] = { 0 };
	byte_slice_t filter = byte_slice_from_array(filter_data);
	ck_assert_int_eq(bloom_nr_bits_set(filter), 0);

	/* These values were picked from /dev/urandom */
	uint32_t values[20] = {
		0x8cccc388, 0x30213665, 0xac26c221, 0xe3a71a13, 0xd0bc3118,
		0x4067c535, 0xf7d8fdb7, 0x4b8105ca, 0xd6558bfe, 0x01b9f37f,
		0x0150a6a3, 0x75f938c3, 0xf0ace4b5, 0x3276877a, 0x4be30a50,
		0x4a2253b9, 0xd22c689d, 0xba937235, 0x66a2af3e, 0x4e0fae61
	};
	for (size_t i = 0; i < 20; i++) {
		ck_assert(!bloom_is_set_single(filter, values[i], 3));
		bloom_set_single(filter, values[i], 3);
		ck_assert(bloom_is_set_single(filter, values[i], 3));
	}

	/* The random numbers display a perfect bloom filter distribution and count approximation */
	ck_assert_uint_eq(bloom_nr_bits_set(filter), 60);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 60), 20);
}
END_TEST

START_TEST(test_count_approximations)
{
	ck_assert_uint_eq(bloom_approx_count(1, 1, 0), 0);
	ck_assert_uint_eq(bloom_approx_count(1, 1, 1), 1);
	ck_assert_uint_eq(bloom_approx_count(1, 1, 2), 2);
	ck_assert_uint_eq(bloom_approx_count(1, 1, 3), 4);
	ck_assert_uint_eq(bloom_approx_count(1, 1, 4), 6);
	ck_assert_uint_eq(bloom_approx_count(1, 1, 5), 8);
	ck_assert_uint_eq(bloom_approx_count(1, 1, 6), 11);
	ck_assert_uint_eq(bloom_approx_count(1, 1, 7), 17);
	ck_assert_uint_eq(bloom_approx_count(1, 1, 8), UINT32_MAX);

	ck_assert_uint_eq(bloom_approx_count(1024, 3, 0), 0);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 8), 3);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 16), 5);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 32), 11);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 64), 21);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 128), 43);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 256), 87);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 512), 176);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 1024), 365);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 2048), 786);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 3072), 1283);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 4096), 1893);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 5120), 2678);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 6144), 3786);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 7168), 5678);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 8190), 22713);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 8191), 24606);
	ck_assert_uint_eq(bloom_approx_count(1024, 3, 8192), UINT32_MAX);

	uint32_t v = bloom_approx_count(1000000000, 3, 999999999);
	ck_assert_uint_ge(v, 356083713 * 0.9);
	ck_assert_uint_le(v, 356083713 * 1.1);

	v = bloom_approx_count(1000000000, 2, 999999999);
	ck_assert_uint_ge(v, 534125570 * 0.9);
	ck_assert_uint_le(v, 534125570 * 1.1);

	v = bloom_approx_count(1000000000, 1, 999999999);
	ck_assert_uint_ge(v, 1068251140 * 0.9);
	ck_assert_uint_le(v, 1068251140 * 1.1);
}
END_TEST

Suite* make_suite(void)
{
	TCase* tc_core = tcase_create("Tests");
	tcase_add_test(tc_core, test_bloom_offsets);
	tcase_add_test(tc_core, test_filter_basics);
	tcase_add_test(tc_core, test_filter_basics_with_overlap);
	tcase_add_test(tc_core, test_filter_fill);
	tcase_add_test(tc_core, test_filter_fill_random);
	tcase_add_test(tc_core, test_count_approximations);

	Suite* s = suite_create("Bloom");
	suite_add_tcase(s, tc_core);
	return s;
}
