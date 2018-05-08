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


#if BYTE_ORDER != LITTLE_ENDIAN
#error The currrent test only work properly on a little endian machine!
#endif

#include "byte_slice.h"

#include <check.h>

/* The 'ck_assert_mem_eq' check is only available in check >= 0.12.0 */
#ifndef ck_assert_mem_eq
#define ck_assert_mem_eq(X, Y, L) ck_assert(memcmp((X),(Y),(L)) == 0)
#endif

START_TEST(test_byte_slice_from_scalar)
{
	uint8_t s1 = 0x01;
	uint16_t s2 = 0x0102;
	uint32_t s3 = 0x01020304;
	uint64_t s4 = 0x0102030405060708;

	byte_slice_t s1_slice = byte_slice_from_scalar(s1);
	ck_assert_uint_eq(s1_slice.len, sizeof(s1));
	ck_assert_ptr_eq(s1_slice.bytes, &s1);
	ck_assert_mem_eq(s1_slice.bytes, &s1, sizeof(s1));

	byte_slice_t s2_slice = byte_slice_from_scalar(s2);
	ck_assert_uint_eq(s2_slice.len, sizeof(s2));
	ck_assert_ptr_eq(s2_slice.bytes, &s2);
	ck_assert_mem_eq(s2_slice.bytes, &s2, sizeof(s2));

	byte_slice_t s3_slice = byte_slice_from_scalar(s3);
	ck_assert_uint_eq(s3_slice.len, sizeof(s3));
	ck_assert_ptr_eq(s3_slice.bytes, &s3);
	ck_assert_mem_eq(s3_slice.bytes, &s3, sizeof(s3));

	byte_slice_t s4_slice = byte_slice_from_scalar(s4);
	ck_assert_uint_eq(s4_slice.len, sizeof(s4));
	ck_assert_ptr_eq(s4_slice.bytes, &s4);
	ck_assert_mem_eq(s4_slice.bytes, &s4, sizeof(s4));
}
END_TEST

START_TEST(test_byte_slice_from_array)
{
	uint8_t a1[3] = { 1, 2, 3 };
	uint64_t a2[3] = { 1, 2, 3 };

	byte_slice_t a1_slice = byte_slice_from_array(a1);
	ck_assert_uint_eq(a1_slice.len, sizeof(a1));
	ck_assert_ptr_eq(a1_slice.bytes, &a1);
	ck_assert_mem_eq(a1_slice.bytes, &a1, sizeof(a1));

	byte_slice_t a2_slice = byte_slice_from_array(a2);
	ck_assert_uint_eq(a2_slice.len, sizeof(a2));
	ck_assert_ptr_eq(a2_slice.bytes, &a2);
	ck_assert_mem_eq(a2_slice.bytes, &a2, sizeof(a2));
}
END_TEST

START_TEST(test_byte_slice_from_ptrlen)
{
	uint8_t a1[3] = { 1, 2, 3 };
	uint64_t a2[3] = { 1, 2, 3 };

	byte_slice_t a1_slice = byte_slice_from_ptrlen(a1, 2);
	ck_assert_uint_eq(a1_slice.len, 2);
	ck_assert_ptr_eq(a1_slice.bytes, &a1);
	ck_assert_mem_eq(a1_slice.bytes, &a1, 2);

	byte_slice_t a2_slice = byte_slice_from_ptrlen(a2, 2);
	ck_assert_uint_eq(a2_slice.len, 16);
	ck_assert_ptr_eq(a2_slice.bytes, &a2);
	ck_assert_mem_eq(a2_slice.bytes, &a2, 16);
}
END_TEST

START_TEST(test_byte_slice_bit_manip)
{
	uint8_t bytes[32] = { 0 };
	byte_slice_t bs = byte_slice_from_array(bytes);
	ck_assert_uint_eq(byte_slice_popcount(bs), 0);

	// set unset bit 0
	ck_assert(!byte_slice_bit_is_set(bs, 0));
	byte_slice_set_bit(bs, 0);
	ck_assert(byte_slice_bit_is_set(bs, 0));
	ck_assert_uint_eq(bytes[0], 1);
	ck_assert_uint_eq(byte_slice_popcount(bs), 1);

	// set unset bit 5
	ck_assert(!byte_slice_bit_is_set(bs, 5));
	byte_slice_set_bit(bs, 5);
	ck_assert(byte_slice_bit_is_set(bs, 5));
	ck_assert_uint_eq(bytes[0], 33);
	ck_assert_uint_eq(byte_slice_popcount(bs), 2);

	// set set bit 5
	byte_slice_set_bit(bs, 5);
	ck_assert(byte_slice_bit_is_set(bs, 5));
	ck_assert_uint_eq(bytes[0], 33);
	ck_assert_uint_eq(byte_slice_popcount(bs), 2);

	// clear set bit 0
	byte_slice_unset_bit(bs, 0);
	ck_assert(!byte_slice_bit_is_set(bs, 0));
	ck_assert_uint_eq(bytes[0], 32);
	ck_assert_uint_eq(byte_slice_popcount(bs), 1);

	// clear unset bit 4
	ck_assert(!byte_slice_bit_is_set(bs, 4));
	byte_slice_unset_bit(bs, 0);
	ck_assert(!byte_slice_bit_is_set(bs, 4));
	ck_assert_uint_eq(bytes[0], 32);
	ck_assert_uint_eq(byte_slice_popcount(bs), 1);

	// set some more bits
	ck_assert(!byte_slice_bit_is_set(bs, 36));
	ck_assert(!byte_slice_bit_is_set(bs, 67));
	ck_assert(!byte_slice_bit_is_set(bs, 98));
	ck_assert(!byte_slice_bit_is_set(bs, 129));
	ck_assert(!byte_slice_bit_is_set(bs, 160));
	ck_assert(!byte_slice_bit_is_set(bs, 198));
	ck_assert(!byte_slice_bit_is_set(bs, 231));
	byte_slice_set_bit(bs, 36);
	byte_slice_set_bit(bs, 67);
	byte_slice_set_bit(bs, 98);
	byte_slice_set_bit(bs, 129);
	byte_slice_set_bit(bs, 160);
	byte_slice_set_bit(bs, 198);
	byte_slice_set_bit(bs, 231);
	ck_assert(byte_slice_bit_is_set(bs, 36));
	ck_assert(byte_slice_bit_is_set(bs, 67));
	ck_assert(byte_slice_bit_is_set(bs, 98));
	ck_assert(byte_slice_bit_is_set(bs, 129));
	ck_assert(byte_slice_bit_is_set(bs, 160));
	ck_assert(byte_slice_bit_is_set(bs, 198));
	ck_assert(byte_slice_bit_is_set(bs, 231));
	ck_assert_uint_eq(byte_slice_popcount(bs), 8);

	{ /* This should be the sequence we've built so far */
		uint32_t ref[8] = { 32, 16, 8, 4, 2, 1, 64, 128 };
		ck_assert_mem_eq(bytes, &ref, 32);
	}

	// Set some more bits in the beginning and at the end
	byte_slice_set_bit(bs, 13);
	byte_slice_set_bit(bs, 21);
	byte_slice_set_bit(bs, 29);
	byte_slice_set_bit(bs, 52);
	byte_slice_set_bit(bs, 246);
	byte_slice_set_bit(bs, 247);
	byte_slice_set_bit(bs, 254);
	byte_slice_set_bit(bs, 255);
	ck_assert_uint_eq(byte_slice_popcount(bs), 16);

	// Check non-aligned popcount results
	ck_assert_uint_eq(byte_slice_popcount(byte_slice(bytes, 31)), 14);
	ck_assert_uint_eq(byte_slice_popcount(byte_slice(bytes, 30)), 12);
	ck_assert_uint_eq(byte_slice_popcount(byte_slice(bytes, 29)), 12);
	ck_assert_uint_eq(byte_slice_popcount(byte_slice(bytes + 1, 31)), 15);
	ck_assert_uint_eq(byte_slice_popcount(byte_slice(bytes + 1, 30)), 13);
	ck_assert_uint_eq(byte_slice_popcount(byte_slice(bytes + 1, 29)), 11);
	ck_assert_uint_eq(byte_slice_popcount(byte_slice(bytes + 1, 28)), 11);
	ck_assert_uint_eq(byte_slice_popcount(byte_slice(bytes + 2, 29)), 12);
	ck_assert_uint_eq(byte_slice_popcount(byte_slice(bytes + 2, 28)), 10);
	ck_assert_uint_eq(byte_slice_popcount(byte_slice(bytes + 2, 27)), 10);
	ck_assert_uint_eq(byte_slice_popcount(byte_slice(bytes + 3, 28)), 11);
	ck_assert_uint_eq(byte_slice_popcount(byte_slice(bytes + 3, 27)), 9);
	ck_assert_uint_eq(byte_slice_popcount(byte_slice(bytes + 3, 26)), 9);

	// Check byte slice clearing
	byte_slice_clear(bs);
	ck_assert_uint_eq(byte_slice_popcount(bs), 0);
}
END_TEST

START_TEST(test_byte_slice_bitwise_or_1)
{
	uint8_t a[16] = "\x01\x03\x05\x07\x09\x0b\x0d\x0f\x11\x13\x15\x17\x19\x1b\x1d\x1f";
	uint8_t b[16] = "\x02\x04\x06\x08\x0a\x0c\x0e\x10\x12\x14\x16\x18\x1a\x1c\x1e\x20";
	byte_slice_bitwise_or(byte_slice_from_ptrlen(a, 16), byte_slice_from_ptrlen(b, 16));
	ck_assert_mem_eq(a, "\x03\x07\x07\x0f\x0b\x0f\x0f\x1f\x13\x17\x17\x1f\x1b\x1f\x1f\x3f", 16);
}
END_TEST

START_TEST(test_byte_slice_bitwise_or_2)
{
	uint8_t a[16] = "\x01\x03\x05\x07\x09\x0b\x0d\x0f\x11\x13\x15\x17\x19\x1b\x1d\x1f";
	uint8_t b[16] = "\x02\x04\x06\x08\x0a\x0c\x0e\x10\x12\x14\x16\x18\x1a\x1c\x1e\x20";
	byte_slice_bitwise_or(byte_slice_from_ptrlen(a, 16), byte_slice_from_ptrlen(b, 15));
	ck_assert_mem_eq(a, "\x03\x07\x07\x0f\x0b\x0f\x0f\x1f\x13\x17\x17\x1f\x1b\x1f\x1f\x1f", 16);
}
END_TEST

START_TEST(test_byte_slice_bitwise_or_3)
{
	uint8_t a[16] = "\x01\x03\x05\x07\x09\x0b\x0d\x0f\x11\x13\x15\x17\x19\x1b\x1d\x1f";
	uint8_t b[16] = "\x02\x04\x06\x08\x0a\x0c\x0e\x10\x12\x14\x16\x18\x1a\x1c\x1e\x20";
	byte_slice_bitwise_or(byte_slice_from_ptrlen(a + 1, 15), byte_slice_from_ptrlen(b + 1, 14));
	ck_assert_mem_eq(a, "\x01\x07\x07\x0f\x0b\x0f\x0f\x1f\x13\x17\x17\x1f\x1b\x1f\x1f\x1f", 16);
}
END_TEST

START_TEST(test_byte_slice_bitwise_and_1)
{
	uint8_t a[16] = "\x01\x03\x05\x07\x09\x0b\x0d\x0f\x11\x13\x15\x17\x19\x1b\x1d\x1f";
	uint8_t b[16] = "\x02\x04\x06\x08\x0a\x0c\x0e\x10\x12\x14\x16\x18\x1a\x1c\x1e\x20";
	byte_slice_bitwise_and(byte_slice_from_ptrlen(a, 16), byte_slice_from_ptrlen(b, 16));
	ck_assert_mem_eq(a, "\x00\x00\x04\x00\x08\x08\x0c\x00\x10\x10\x14\x10\x18\x18\x1c\x00", 16);
}
END_TEST

START_TEST(test_byte_slice_bitwise_and_2)
{
	uint8_t a[16] = "\x01\x03\x05\x07\x09\x0b\x0d\x0f\x11\x13\x15\x17\x19\x1b\x1d\x1f";
	uint8_t b[16] = "\x02\x04\x06\x08\x0a\x0c\x0e\x10\x12\x14\x16\x18\x1a\x1c\x1e\x20";
	byte_slice_bitwise_and(byte_slice_from_ptrlen(a, 16), byte_slice_from_ptrlen(b, 15));
	ck_assert_mem_eq(a, "\x00\x00\x04\x00\x08\x08\x0c\x00\x10\x10\x14\x10\x18\x18\x1c\x1f", 16);
}
END_TEST

START_TEST(test_byte_slice_bitwise_and_3)
{
	uint8_t a[16] = "\x01\x03\x05\x07\x09\x0b\x0d\x0f\x11\x13\x15\x17\x19\x1b\x1d\x1f";
	uint8_t b[16] = "\x02\x04\x06\x08\x0a\x0c\x0e\x10\x12\x14\x16\x18\x1a\x1c\x1e\x20";
	byte_slice_bitwise_and(byte_slice_from_ptrlen(a + 1, 15), byte_slice_from_ptrlen(b + 1, 14));
	ck_assert_mem_eq(a, "\x01\x00\x04\x00\x08\x08\x0c\x00\x10\x10\x14\x10\x18\x18\x1c\x1f", 16);
}
END_TEST

START_TEST(test_byte_slice_mul32)
{
	uint8_t a[16]  = {   1,   2,   3,   4,   2,   3,   4,   5,   3,   4,   5,   6,   6,   7,   8,   9 };
	uint8_t r1[16] = {   3,   6,   9,  12,   6,   9,  12,  15,   9,  12,  15,  18,  18,  21,  24,  27 };
	uint8_t r2[16] = {   9,  18,  27,  36,  18,  27,  36,  45,  27,  36,  45,  54,  54,  63,  72,  81 };
	uint8_t r3[16] = {  27,  54,  81, 108,  54,  81, 108, 135,  81, 108, 135, 162, 162, 189, 216, 243 };
	uint8_t r4[16] = {  81, 162, 243,  68, 163, 243,  68, 150, 244,  68, 150, 231, 231,  56, 138, 219 };

	ck_assert_uint_eq(byte_slice_mul32(byte_slice_from_array(a), 3), 0);
	ck_assert_mem_eq(a, r1, 16);

	ck_assert_uint_eq(byte_slice_mul32(byte_slice_from_array(a), 3), 0);
	ck_assert_mem_eq(a, r2, 16);

	ck_assert_uint_eq(byte_slice_mul32(byte_slice_from_array(a), 3), 0);
	ck_assert_mem_eq(a, r3, 16);

	// Starts overflowing..
	ck_assert_uint_eq(byte_slice_mul32(byte_slice_from_array(a), 3), 2);
	ck_assert_mem_eq(a, r4, 16);
}
END_TEST

#ifdef HAS_BYTE_SLICE_MUL64
START_TEST(test_byte_slice_mul64)
{
	uint8_t a[16]  = {   1,   2,   3,   4,   2,   3,   4,   5,   3,   4,   5,   6,   6,   7,   8,   9 };
	uint8_t r1[16] = {   3,   6,   9,  12,   6,   9,  12,  15,   9,  12,  15,  18,  18,  21,  24,  27 };
	uint8_t r2[16] = {   9,  18,  27,  36,  18,  27,  36,  45,  27,  36,  45,  54,  54,  63,  72,  81 };
	uint8_t r3[16] = {  27,  54,  81, 108,  54,  81, 108, 135,  81, 108, 135, 162, 162, 189, 216, 243 };
	uint8_t r4[16] = {  81, 162, 243,  68, 163, 243,  68, 150, 244,  68, 150, 231, 231,  56, 138, 219 };

	ck_assert_uint_eq(byte_slice_mul64(byte_slice_from_array(a), 3), 0);
	ck_assert_mem_eq(a, r1, 16);

	ck_assert_uint_eq(byte_slice_mul64(byte_slice_from_array(a), 3), 0);
	ck_assert_mem_eq(a, r2, 16);

	ck_assert_uint_eq(byte_slice_mul64(byte_slice_from_array(a), 3), 0);
	ck_assert_mem_eq(a, r3, 16);

	// Starts overflowing..
	ck_assert_uint_eq(byte_slice_mul64(byte_slice_from_array(a), 3), 2);
	ck_assert_mem_eq(a, r4, 16);
}
END_TEST
#endif

Suite* make_suite(void)
{
	TCase* tc_core = tcase_create("Tests");
	tcase_add_test(tc_core, test_byte_slice_from_scalar);
	tcase_add_test(tc_core, test_byte_slice_from_array);
	tcase_add_test(tc_core, test_byte_slice_from_ptrlen);

	tcase_add_test(tc_core, test_byte_slice_bit_manip);
	tcase_add_test(tc_core, test_byte_slice_bitwise_or_1);
	tcase_add_test(tc_core, test_byte_slice_bitwise_or_2);
	tcase_add_test(tc_core, test_byte_slice_bitwise_or_3);
	tcase_add_test(tc_core, test_byte_slice_bitwise_and_1);
	tcase_add_test(tc_core, test_byte_slice_bitwise_and_2);
	tcase_add_test(tc_core, test_byte_slice_bitwise_and_3);

	tcase_add_test(tc_core, test_byte_slice_mul32);
#ifdef HAS_BYTE_SLICE_MUL64
	tcase_add_test(tc_core, test_byte_slice_mul64);
#endif

	Suite* s = suite_create("Bloom");
	suite_add_tcase(s, tc_core);
	return s;
}
