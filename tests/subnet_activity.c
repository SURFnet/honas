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

#include <check.h>
#include "subnet_activity.h"

#define TEST_SUBNET_ACTIVITY_FILE "/home/gijs/honas/etc/example_subnet_definitions.json"

// Print debug output if requested.
void print_debug(struct subnet_activity* p_subact)
{
	// Some printing for debugging...
	for (unsigned int i = 0; i < p_subact->registered_entities; ++i)
	{
		printf("Entity %s was registered.\n", p_subact->entities[i]->name);
	}

	struct prefix_match* pf_it = NULL;
	struct prefix_match* tmp_it = NULL;
	HASH_ITER(hh, p_subact->prefixes, pf_it, tmp_it)
	{
		char addrstr[64];
		inet_ntop(pf_it->prefix.address.af, &pf_it->prefix.address.in, addrstr, sizeof(addrstr));
		printf("Prefix %s/%i was registered to entity %s.\n", addrstr, pf_it->prefix.length, pf_it->associated_entity->name);
	}
}

START_TEST(test_subnet_activity)
{
	struct subnet_activity subnet_act;

	// Create a new subnet activity state.
	ck_assert_int_eq(subnet_activity_initialize(TEST_SUBNET_ACTIVITY_FILE, &subnet_act), SA_OK);

	//print_debug(&subnet_act);

	// Check if the correct number of registrations took place.
	ck_assert_int_eq(subnet_act.registered_entities, 2);
	ck_assert_int_eq(subnet_act.registered_prefixes, 7);

	// Check the prefix bounds.
	ck_assert_int_eq(subnet_act.shortest_ipv4_prefix, 8);
	ck_assert_int_eq(subnet_act.longest_ipv4_prefix, 24);
	ck_assert_int_eq(subnet_act.longest_ipv6_prefix, 64);
	ck_assert_int_eq(subnet_act.shortest_ipv6_prefix, 48);

	// Check for the presence of a few subnets that should be present.
	struct prefix_match* pf_match = NULL;
	struct in_addr46 pf = { 0 };
	pf.af = AF_INET;
	parse_ipv4("192.87.0.1", &pf.in.addr4, 0);
	ck_assert_int_eq(subnet_activity_match_prefix(&pf, &subnet_act, &pf_match), SA_OK);
	ck_assert(pf_match != NULL);
	ck_assert_str_eq(pf_match->associated_entity->name, "SURFnet");

	parse_ipv4("145.0.3.6", &pf.in.addr4, 0);
	ck_assert_int_eq(subnet_activity_match_prefix(&pf, &subnet_act, &pf_match), SA_OK);
	ck_assert(pf_match != NULL);
	ck_assert_str_eq(pf_match->associated_entity->name, "SURFnet");

	parse_ipv4("192.42.113.120", &pf.in.addr4, 0);
	ck_assert_int_eq(subnet_activity_match_prefix(&pf, &subnet_act, &pf_match), SA_OK);
	ck_assert(pf_match != NULL);
	ck_assert_str_eq(pf_match->associated_entity->name, "netSURF");

	parse_ipv4("192.42.113.102", &pf.in.addr4, 0);
	ck_assert_int_eq(subnet_activity_match_prefix(&pf, &subnet_act, &pf_match), SA_OK);
	ck_assert(pf_match != NULL);
	ck_assert_str_eq(pf_match->associated_entity->name, "netSURF");

	parse_ipv4("145.220.20.20", &pf.in.addr4, 0);
	ck_assert_int_eq(subnet_activity_match_prefix(&pf, &subnet_act, &pf_match), SA_OK);
	ck_assert(pf_match != NULL);
	ck_assert_str_eq(pf_match->associated_entity->name, "netSURF");

	pf.af = AF_INET6;
	parse_ipv6("2001:67c:6ec:201:145:220:0:1", &pf.in.addr6, 0);
	ck_assert_int_eq(subnet_activity_match_prefix(&pf, &subnet_act, &pf_match), SA_OK);
	ck_assert(pf_match != NULL);
	ck_assert_str_eq(pf_match->associated_entity->name, "netSURF");

	// Check for prefixes that should not exist.
	parse_ipv6("2001:678:230:2123:192:42:123:139", &pf.in.addr6, 0);
	ck_assert_int_eq(subnet_activity_match_prefix(&pf, &subnet_act, &pf_match), SA_OK);
	ck_assert(pf_match == NULL);

	parse_ipv6("2001:610:510:123:192:42:123:139", &pf.in.addr6, 0);
	ck_assert_int_eq(subnet_activity_match_prefix(&pf, &subnet_act, &pf_match), SA_OK);
	ck_assert(pf_match == NULL);

	pf.af = AF_INET;
	parse_ipv4("8.8.8.8", &pf.in.addr4, 0);
	ck_assert_int_eq(subnet_activity_match_prefix(&pf, &subnet_act, &pf_match), SA_OK);
	ck_assert(pf_match == NULL);

	// Destroy the subnet activity state.
	ck_assert_int_eq(subnet_activity_destroy(&subnet_act), SA_OK);
}
END_TEST

Suite* make_suite(void)
{
	TCase* tc_core = tcase_create("Tests");
	tcase_add_test(tc_core, test_subnet_activity);

	Suite* s = suite_create("Honas Subnet Activity");
	suite_add_tcase(s, tc_core);
	return s;
}
