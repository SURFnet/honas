/*
 * Copyright (c) 2018, Gijs Rijnders, SURFnet
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

#include "honas_state.h"

#include <check.h>
#include <openssl/sha.h>

#define SHA256_STRING_LENGTH	64

static bool _decode_nibble(char c, int* r)
{
	if (c >= '0' && c <= '9') {
		*r = c - '0';
	} else if (c >= 'A' && c <= 'F') {
		*r = c - 'A' + 10;
	} else if (c >= 'a' && c <= 'f') {
		*r = c - 'a' + 10;
	} else {
		return false;
	}
	return true;
}

static bool decode_string_hex(const char* str, size_t strlen, uint8_t* dec, size_t declen)
{
	if (strlen != declen * 2)
		return false;

	int high, low;
	for (size_t si = 0, di = 0; di < declen; si++, di++) {
		if (!_decode_nibble(str[si], &high) || !_decode_nibble(str[++si], &low))
			return false;
		dec[di] = (high << 4) | low;
	}
	return true;
}

START_TEST(test_aggregate_states)
{
	honas_state_t first_state = { 0 };
	honas_state_t second_state = { 0 };
	struct in_addr46 client = { 0 };
	client.af = AF_INET;
	const unsigned int addr = 0xDE329823;
	memcpy(&client.in.addr4, &addr, sizeof(unsigned int));

	// Create two states having slightly different elements.
	honas_state_create(&first_state, 1, 1024 * 1024, 10, 1, 1);
	honas_state_create(&second_state, 1, 1024 * 1024, 10, 1, 1);

	// Add a few domain names to the first state.
	honas_state_register_host_name_lookup(&first_state, time(NULL), &client
		, (uint8_t*)"google.com", strlen("google.com"), (uint8_t*)"Google Inc", strlen("Google Inc"));
	honas_state_register_host_name_lookup(&first_state, time(NULL), &client
		, (uint8_t*)"surfnet.nl", strlen("surfnet.nl"), NULL, 0);
	honas_state_register_host_name_lookup(&first_state, time(NULL), &client
		, (uint8_t*)"unbound.prutsnet.nl", strlen("unbound.prutsnet.nl"), NULL, 0);

	// Add a few different domain names to the second state.
	honas_state_register_host_name_lookup(&second_state, time(NULL), &client
		, (uint8_t*)"surf.net", strlen("surf.net"), NULL, 0);
	honas_state_register_host_name_lookup(&second_state, time(NULL), &client
		, (uint8_t*)"sidn.nl", strlen("sidn.nl"), (uint8_t*)"netSURF", strlen("netSURF"));

	// Perform a lookup for all added domain names.
	uint8_t bytes[SHA256_STRING_LENGTH / 2];
	ck_assert(decode_string_hex("a3a410b1235a4d97fbcd7c0fff1a947d6ca8a62756dfd016e74e9a69d69b38a0"
		, SHA256_STRING_LENGTH, bytes, sizeof(bytes)));
	const uint32_t d1 = honas_state_check_host_name_lookups(&first_state, byte_slice_from_array(bytes), NULL);
	ck_assert(decode_string_hex("94a2d36558fd752ad70b303057a24c6ed61758e1f3eb63402651ca6820ba6c09"
		, SHA256_STRING_LENGTH, bytes, sizeof(bytes)));
	const uint32_t d2 = honas_state_check_host_name_lookups(&first_state
		, byte_slice_from_array(bytes), NULL);

	// Only test the SLD.TLD in this check.
	ck_assert(decode_string_hex("efcd513fc9fea11685641b98b3285b8500c4fe7cfd94b75b07a5ab89b9619f5e"
		, SHA256_STRING_LENGTH, bytes, sizeof(bytes)));
	const uint32_t d3 = honas_state_check_host_name_lookups(&first_state
		, byte_slice_from_array(bytes), NULL);
	ck_assert(decode_string_hex("1482827f78f8a35cb8297bea7e9ad832780828248ae168828fdbd9438622294a"
		, SHA256_STRING_LENGTH, bytes, sizeof(bytes)));
	const uint32_t d4 = honas_state_check_host_name_lookups(&first_state
		, byte_slice_from_array(bytes), NULL);

	// Entity netSURF is prepended to domain name sidn.nl here.
	ck_assert(decode_string_hex("df944e735420166e5e443d57a6fb89f05302aff2dba6029364614174a929362d"
		, SHA256_STRING_LENGTH, bytes, sizeof(bytes)));
	const uint32_t d5 = honas_state_check_host_name_lookups(&first_state
		, byte_slice_from_array(bytes), NULL);

	// Query the last two domain names in the second filter.
	ck_assert(decode_string_hex("d4c9d9027326271a89ce51fcaf328ed673f17be33469ff979e8ab8dd501e664f"
		, SHA256_STRING_LENGTH, bytes, sizeof(bytes)));
	const uint32_t d1_2 = honas_state_check_host_name_lookups(&second_state
		, byte_slice_from_array(bytes), NULL);
	ck_assert(decode_string_hex("94a2d36558fd752ad70b303057a24c6ed61758e1f3eb63402651ca6820ba6c09"
		, SHA256_STRING_LENGTH, bytes, sizeof(bytes)));
	const uint32_t d2_2 = honas_state_check_host_name_lookups(&second_state
		, byte_slice_from_array(bytes), NULL);
	ck_assert(decode_string_hex("82f248bb3116b20106723aba16c51cf248e45135e87509e86a8eda244634cd85"
		, SHA256_STRING_LENGTH, bytes, sizeof(bytes)));
	const uint32_t d3_2 = honas_state_check_host_name_lookups(&second_state
		, byte_slice_from_array(bytes), NULL);
	ck_assert(decode_string_hex("1482827f78f8a35cb8297bea7e9ad832780828248ae168828fdbd9438622294a"
		, SHA256_STRING_LENGTH, bytes, sizeof(bytes)));
	const uint32_t d4_2 = honas_state_check_host_name_lookups(&second_state
		, byte_slice_from_array(bytes), NULL);
	ck_assert(decode_string_hex("df944e735420166e5e443d57a6fb89f05302aff2dba6029364614174a929362d"
		, SHA256_STRING_LENGTH, bytes, sizeof(bytes)));
	const uint32_t d5_2 = honas_state_check_host_name_lookups(&second_state
		, byte_slice_from_array(bytes), NULL);

	// Check whether the results are correct.
	ck_assert_int_gt(d1, 0);
	ck_assert_int_gt(d2, 0);
	ck_assert_int_gt(d3, 0);

	// The last two domain names should not reside in the first state.
	ck_assert_int_eq(d4, 0);
	ck_assert_int_eq(d5, 0);

	// However, they should reside in the second state.
	ck_assert_int_gt(d4_2, 0);
	ck_assert_int_gt(d5_2, 0);

	// And the first three domain names should not reside in the second state.
	ck_assert_int_eq(d1_2, 0);
	ck_assert_int_eq(d2_2, 0);
	ck_assert_int_eq(d3_2, 0);

	// Now aggregate the states, taking the union.
	const bool agg_res = honas_state_aggregate_combine(&first_state, &second_state);
	ck_assert(agg_res == true);

	// Now check whether all domain names reside in the first state.
	decode_string_hex("d4c9d9027326271a89ce51fcaf328ed673f17be33469ff979e8ab8dd501e664f"
		, SHA256_STRING_LENGTH, bytes, sizeof(bytes));
	const uint32_t d1_final = honas_state_check_host_name_lookups(&first_state
		, byte_slice_from_array(bytes), NULL);
	decode_string_hex("94a2d36558fd752ad70b303057a24c6ed61758e1f3eb63402651ca6820ba6c09"
		, SHA256_STRING_LENGTH, bytes, sizeof(bytes));
	const uint32_t d2_final = honas_state_check_host_name_lookups(&first_state
		, byte_slice_from_array(bytes), NULL);
	decode_string_hex("82f248bb3116b20106723aba16c51cf248e45135e87509e86a8eda244634cd85"
		, SHA256_STRING_LENGTH, bytes, sizeof(bytes));
	const uint32_t d3_final = honas_state_check_host_name_lookups(&first_state
		, byte_slice_from_array(bytes), NULL);
	decode_string_hex("1482827f78f8a35cb8297bea7e9ad832780828248ae168828fdbd9438622294a"
		, SHA256_STRING_LENGTH, bytes, sizeof(bytes));
	const uint32_t d4_final = honas_state_check_host_name_lookups(&first_state
		, byte_slice_from_array(bytes), NULL);
	decode_string_hex("17ca13c1678c969e01690643494ae1e4faabd96801da50442dcb173c294594f3"
		, SHA256_STRING_LENGTH, bytes, sizeof(bytes));
	const uint32_t d5_final = honas_state_check_host_name_lookups(&first_state
		, byte_slice_from_array(bytes), NULL);

	ck_assert_int_gt(d1_final, 0);
	ck_assert_int_gt(d2_final, 0);
	ck_assert_int_gt(d3_final, 0);
	ck_assert_int_gt(d4_final, 0);
	ck_assert_int_gt(d5_final, 0);

	// Destroy the states.
	honas_state_destroy(&first_state);
	honas_state_destroy(&second_state);
}
END_TEST

Suite* make_suite(void)
{
	TCase* tc_core = tcase_create("Tests");
	tcase_add_test(tc_core, test_aggregate_states);

	Suite* s = suite_create("Honas State Aggregation");
	suite_add_tcase(s, tc_core);
	return s;
}
