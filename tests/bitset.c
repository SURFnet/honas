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

#include "bitset.h"

#include <check.h>

START_TEST(test_bitset_create)
{
	bitset_t bs;

	// Minimal bitset
	bitset_create(&bs, 1);
	ck_assert(!bitset_using_byte_slice(&bs));
	ck_assert_uint_eq(bs.num_bits, 1);
	ck_assert_uint_eq(bs.repr.ints[0], 0);
	ck_assert_uint_eq(bs.repr.ints[1], 0);
	ck_assert_uint_eq(bitset_popcount(&bs), 0);
	ck_assert_ptr_eq(bitset_as_byte_slice(&bs).bytes, &bs.repr.ints);
	bitset_destroy(&bs);

	// Maximum inline bitset
	bitset_create(&bs, sizeof(size_t) * 2 * 8);
	ck_assert(!bitset_using_byte_slice(&bs));
	ck_assert_uint_eq(bs.num_bits, sizeof(size_t) * 2 * 8);
	ck_assert_uint_eq(bs.repr.ints[0], 0);
	ck_assert_uint_eq(bs.repr.ints[1], 0);
	ck_assert_uint_eq(bitset_popcount(&bs), 0);
	ck_assert_ptr_eq(bitset_as_byte_slice(&bs).bytes, &bs.repr.ints);
	bitset_destroy(&bs);

	// Minimum allocated bitset
	bitset_create(&bs, sizeof(size_t) * 2 * 8 + 1);
	ck_assert(bitset_using_byte_slice(&bs));
	ck_assert_uint_eq(bs.num_bits, sizeof(size_t) * 2 * 8 + 1);
	ck_assert_uint_eq(bs.repr.byte_slice.len, sizeof(size_t) * 2 + 1);
	ck_assert_ptr_ne(bs.repr.byte_slice.bytes, NULL);
	ck_assert_uint_eq(bitset_popcount(&bs), 0);
	ck_assert_ptr_eq(bitset_as_byte_slice(&bs).bytes, bs.repr.byte_slice.bytes);
	bitset_destroy(&bs);
	ck_assert_ptr_eq(bs.repr.byte_slice.bytes, NULL);
}
END_TEST

START_TEST(test_bitset_inline_bit_manip)
{
	bitset_t bs;
	bitset_create(&bs, 8 * 8);
	ck_assert_uint_eq(bitset_popcount(&bs), 0);

	// set unset bit 0
	ck_assert(!bitset_bit_is_set(&bs, 0));
	bitset_set_bit(&bs, 0);
	ck_assert(bitset_bit_is_set(&bs, 0));
	ck_assert_uint_eq(bs.repr.ints[0], 1);
	ck_assert_uint_eq(bitset_popcount(&bs), 1);

	// set unset bit 5
	ck_assert(!bitset_bit_is_set(&bs, 5));
	bitset_set_bit(&bs, 5);
	ck_assert(bitset_bit_is_set(&bs, 5));
	ck_assert_uint_eq(bs.repr.ints[0], 33);
	ck_assert_uint_eq(bitset_popcount(&bs), 2);

	// set set bit 5
	bitset_set_bit(&bs, 5);
	ck_assert(bitset_bit_is_set(&bs, 5));
	ck_assert_uint_eq(bs.repr.ints[0], 33);
	ck_assert_uint_eq(bitset_popcount(&bs), 2);

	// clear set bit 0
	bitset_unset_bit(&bs, 0);
	ck_assert(!bitset_bit_is_set(&bs, 0));
	ck_assert_uint_eq(bs.repr.ints[0], 32);
	ck_assert_uint_eq(bitset_popcount(&bs), 1);

	// clear unset bit 4
	ck_assert(!bitset_bit_is_set(&bs, 4));
	bitset_unset_bit(&bs, 0);
	ck_assert(!bitset_bit_is_set(&bs, 4));
	ck_assert_uint_eq(bs.repr.ints[0], 32);
	ck_assert_uint_eq(bitset_popcount(&bs), 1);

	// set some more bits
	ck_assert(!bitset_bit_is_set(&bs, 12));
	ck_assert(!bitset_bit_is_set(&bs, 19));
	ck_assert(!bitset_bit_is_set(&bs, 26));
	ck_assert(!bitset_bit_is_set(&bs, 33));
	ck_assert(!bitset_bit_is_set(&bs, 40));
	ck_assert(!bitset_bit_is_set(&bs, 54));
	ck_assert(!bitset_bit_is_set(&bs, 63));
	bitset_set_bit(&bs, 12);
	bitset_set_bit(&bs, 19);
	bitset_set_bit(&bs, 26);
	bitset_set_bit(&bs, 33);
	bitset_set_bit(&bs, 40);
	bitset_set_bit(&bs, 54);
	bitset_set_bit(&bs, 63);
	ck_assert(bitset_bit_is_set(&bs, 12));
	ck_assert(bitset_bit_is_set(&bs, 19));
	ck_assert(bitset_bit_is_set(&bs, 26));
	ck_assert(bitset_bit_is_set(&bs, 33));
	ck_assert(bitset_bit_is_set(&bs, 40));
	ck_assert(bitset_bit_is_set(&bs, 54));
	ck_assert(bitset_bit_is_set(&bs, 63));
	ck_assert_uint_eq(bitset_popcount(&bs), 8);

	{ /* This should be the sequence we've built so far */
		uint8_t ref[8] = { 32, 16, 8, 4, 2, 1, 64, 128 };
		for (size_t i = 0; i < 8; i++)
			ck_assert_uint_eq(((uint8_t*)bs.repr.ints)[i], ref[i]);
	}

	// Check byte slice clearing
	bitset_clear(&bs);
	ck_assert_uint_eq(bitset_popcount(&bs), 0);

	bitset_destroy(&bs);
}
END_TEST

START_TEST(test_bitset_inline_bitwise_or)
{
	bitset_t a, b;
	bitset_create(&a, sizeof(size_t) * 2 * 8);
	bitset_create(&b, sizeof(size_t) * 2 * 8);
	a.repr.ints[0] = 0x01030507;
	a.repr.ints[1] = 0x090b0d0f;
	b.repr.ints[0] = 0x02040608;
	b.repr.ints[1] = 0x0a0c0e10;
	bitset_bitwise_or(&a, &b);
	ck_assert_uint_eq(a.repr.ints[0], 0x0307070f);
	ck_assert_uint_eq(a.repr.ints[1], 0x0b0f0f1f);
	bitset_destroy(&a);
	bitset_destroy(&b);
}
END_TEST

START_TEST(test_bitset_inline_bitwise_and)
{
	bitset_t a, b;
	bitset_create(&a, sizeof(size_t) * 2 * 8);
	bitset_create(&b, sizeof(size_t) * 2 * 8);
	a.repr.ints[0] = 0x01030507;
	a.repr.ints[1] = 0x090b0d0f;
	b.repr.ints[0] = 0x02040608;
	b.repr.ints[1] = 0x0a0c0e10;
	bitset_bitwise_and(&a, &b);
	ck_assert_uint_eq(a.repr.ints[0], 0x00000400);
	ck_assert_uint_eq(a.repr.ints[1], 0x08080c00);
	bitset_destroy(&a);
	bitset_destroy(&b);
}
END_TEST

START_TEST(test_bitset_alloc_bit_manip)
{
	bitset_t bs;
	bitset_create(&bs, 32 * 8);
	ck_assert_uint_eq(bitset_popcount(&bs), 0);

	// set unset bit 0
	ck_assert(!bitset_bit_is_set(&bs, 0));
	bitset_set_bit(&bs, 0);
	ck_assert(bitset_bit_is_set(&bs, 0));
	ck_assert_uint_eq(bs.repr.byte_slice.bytes[0], 1);
	ck_assert_uint_eq(bitset_popcount(&bs), 1);

	// set unset bit 5
	ck_assert(!bitset_bit_is_set(&bs, 5));
	bitset_set_bit(&bs, 5);
	ck_assert(bitset_bit_is_set(&bs, 5));
	ck_assert_uint_eq(bs.repr.byte_slice.bytes[0], 33);
	ck_assert_uint_eq(bitset_popcount(&bs), 2);

	// set set bit 5
	bitset_set_bit(&bs, 5);
	ck_assert(bitset_bit_is_set(&bs, 5));
	ck_assert_uint_eq(bs.repr.byte_slice.bytes[0], 33);
	ck_assert_uint_eq(bitset_popcount(&bs), 2);

	// clear set bit 0
	bitset_unset_bit(&bs, 0);
	ck_assert(!bitset_bit_is_set(&bs, 0));
	ck_assert_uint_eq(bs.repr.byte_slice.bytes[0], 32);
	ck_assert_uint_eq(bitset_popcount(&bs), 1);

	// clear unset bit 4
	ck_assert(!bitset_bit_is_set(&bs, 4));
	bitset_unset_bit(&bs, 0);
	ck_assert(!bitset_bit_is_set(&bs, 4));
	ck_assert_uint_eq(bs.repr.byte_slice.bytes[0], 32);
	ck_assert_uint_eq(bitset_popcount(&bs), 1);

	// set some more bits
	ck_assert(!bitset_bit_is_set(&bs, 36));
	ck_assert(!bitset_bit_is_set(&bs, 67));
	ck_assert(!bitset_bit_is_set(&bs, 98));
	ck_assert(!bitset_bit_is_set(&bs, 129));
	ck_assert(!bitset_bit_is_set(&bs, 160));
	ck_assert(!bitset_bit_is_set(&bs, 198));
	ck_assert(!bitset_bit_is_set(&bs, 231));
	bitset_set_bit(&bs, 36);
	bitset_set_bit(&bs, 67);
	bitset_set_bit(&bs, 98);
	bitset_set_bit(&bs, 129);
	bitset_set_bit(&bs, 160);
	bitset_set_bit(&bs, 198);
	bitset_set_bit(&bs, 231);
	ck_assert(bitset_bit_is_set(&bs, 36));
	ck_assert(bitset_bit_is_set(&bs, 67));
	ck_assert(bitset_bit_is_set(&bs, 98));
	ck_assert(bitset_bit_is_set(&bs, 129));
	ck_assert(bitset_bit_is_set(&bs, 160));
	ck_assert(bitset_bit_is_set(&bs, 198));
	ck_assert(bitset_bit_is_set(&bs, 231));
	ck_assert_uint_eq(bitset_popcount(&bs), 8);

	{ /* This should be the sequence we've built so far */
		uint32_t ref[8] = { 32, 16, 8, 4, 2, 1, 64, 128 };
		for (size_t i = 0; i < 8; i++)
			ck_assert_uint_eq(((uint32_t*)bs.repr.byte_slice.bytes)[i], ref[i]);
	}

	// Set some more bits in the beginning and at the end
	bitset_set_bit(&bs, 13);
	bitset_set_bit(&bs, 21);
	bitset_set_bit(&bs, 29);
	bitset_set_bit(&bs, 52);
	bitset_set_bit(&bs, 246);
	bitset_set_bit(&bs, 247);
	bitset_set_bit(&bs, 254);
	bitset_set_bit(&bs, 255);
	ck_assert_uint_eq(bitset_popcount(&bs), 16);

	// Check byte slice clearing
	bitset_clear(&bs);
	ck_assert_uint_eq(bitset_popcount(&bs), 0);

	bitset_destroy(&bs);
}
END_TEST

Suite* make_suite(void)
{
	TCase* tc_core = tcase_create("Tests");
	tcase_add_test(tc_core, test_bitset_create);
	tcase_add_test(tc_core, test_bitset_inline_bit_manip);
	tcase_add_test(tc_core, test_bitset_inline_bitwise_or);
	tcase_add_test(tc_core, test_bitset_inline_bitwise_and);
	tcase_add_test(tc_core, test_bitset_alloc_bit_manip);

	Suite* s = suite_create("Bloom");
	suite_add_tcase(s, tc_core);
	return s;
}
