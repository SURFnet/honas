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

#include "../src/input_dns_relayd.c"

#include <check.h>

START_TEST(test_init_destroy)
{
	input_dns_relayd_t* state = NULL;
	input_dns_relayd_init(&state);
	input_dns_relayd_destroy(state);
}
END_TEST

#define my_parse_ok(line, client, host_name)                                                                                                                                                                                                               \
	do {                                                                                                                                                                                                                                                   \
		char _line[] = line;                                                                                                                                                                                                                               \
		struct in_addr46 _client_result;                                                                                                                                                                                                                   \
		uint8_t* _host_name_result;                                                                                                                                                                                                                           \
		size_t _host_name_result_length;                                                                                                                                                                                                                 \
		ck_assert_msg(input_dns_relayd_parse_line(state, _line, sizeof(_line) - 1, &_client_result, &_host_name_result, &_host_name_result_length), "Failed to parse line: " line);                                                                        \
		ck_assert_msg(strcmp(client, str_in_addr(&_client_result)) == 0, "Failed to parse correct client from line: " line " (expected: %s, got: %s)", client, str_in_addr(&_client_result));                                                              \
		ck_assert_msg(strlen(host_name) == _host_name_result_length && memcmp(host_name, _host_name_result, _host_name_result_length) == 0, "Failed to parse correct hostname from line: " line " (expected: %s, got: %s)", host_name, _host_name_result); \
	} while (0)
#define my_parse_fail(line)                                                                                                                                                                                     \
	do {                                                                                                                                                                                                        \
		char _line[] = line;                                                                                                                                                                                    \
		struct in_addr46 _client_result;                                                                                                                                                                        \
		uint8_t* _host_name_result;                                                                                                                                                                                \
		size_t _host_name_result_length;                                                                                                                                                                      \
		ck_assert_msg(!input_dns_relayd_parse_line(state, _line, sizeof(_line) - 1, &_client_result, &_host_name_result, &_host_name_result_length), "Successfully parsed line that should have failed " line); \
	} while (0)

START_TEST(test_parse_input_line)
{
	input_dns_relayd_t* state = NULL;
	input_dns_relayd_init(&state);

	/* Check that some basic lines are ignored */
	my_parse_fail("");
	my_parse_fail("#");
	my_parse_fail("# this is a comment line");

	/* Check that invalid input fails to parse */
	my_parse_fail("some garbage input line");
	my_parse_fail("1 1.2.3.4 foo.nl./1/1");
	my_parse_fail("1000000000 client.some.domain.com foo.nl./1/1");
	my_parse_fail("1000000000 100.200.300.400 foo.nl./1/1");
	my_parse_fail("1000000000 1.2.3.4 wrong_name.test./1/1");
	my_parse_fail("1000000000 1.2.3.4 valid.test.");
	my_parse_fail("1000000000 1.2.3.4 valid.test./1");
	my_parse_fail("1000000000 1.2.3.4 valid.test./1/");
	my_parse_fail("1000000000 1.2.3.4 valid.test./1/1/");
	my_parse_fail("1000000000 1.2.3.4 valid.test./1/1/1");
	my_parse_fail("1000000000 1.2.3.4 valid.test./123456/1");
	my_parse_fail("1000000000 1.2.3.4 valid.test./1/123456");

	/* Check certain classes and requests are ignored */
	my_parse_fail("1000000000 1.2.3.4 foo.nl./1/2");
	my_parse_fail("1000000000 1.2.3.4 foo.nl./1/12");
	my_parse_fail("1000000000 1.2.3.4 foo.nl./3/1");
	my_parse_fail("1000000000 1.2.3.4 foo.nl./18/1");

	/* Check succesful parses of request type A */
	my_parse_ok("1000000000 1.2.3.4 foo.nl./1/1", "1.2.3.4", "foo.nl.");
	my_parse_ok("1000000000 fe80::ff:feed:f00d test.example.com./1/1", "fe80::ff:feed:f00d", "test.example.com.");

	/* Some more valid parses of other request types */
	my_parse_ok("1000000000 1.2.3.4 foo.nl./2/1", "1.2.3.4", "foo.nl."); /* NS */
	my_parse_ok("1000000000 1.2.3.4 foo.nl./15/1", "1.2.3.4", "foo.nl."); /* MX */
	my_parse_ok("1000000000 1.2.3.4 foo.nl./28/1", "1.2.3.4", "foo.nl."); /* AAAA */

	input_dns_relayd_destroy(state);
}
END_TEST

Suite* make_suite(void)
{
	TCase* tc_core = tcase_create("Tests");
	tcase_add_test(tc_core, test_init_destroy);
	tcase_add_test(tc_core, test_parse_input_line);

	Suite* s = suite_create("Honas input dns-relayd");
	suite_add_tcase(s, tc_core);
	return s;
}
