/*
 * Copyright (c) 2018 Gijs Rijnders, SURFnet
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

#include "subnet_activity.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <yajl/yajl_parse.h>

#define READ_BLOCK_SIZE 65536

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

// All 32 possible IPv4 prefix masks for quick access.
static const unsigned long all_ipv4_subnet_masks[] = {
	0x00000000, 0x00000080, 0x000000C0, 0x000000E0, 0x000000F0, 0x000000F8, 0x000000FC, 0x000000FE,
	0x000000FF, 0x000080FF, 0x0000C0FF, 0x0000E0FF, 0x0000F0FF, 0x0000F8FF, 0x0000FCFF, 0x0000FEFF,
	0x0000FFFF, 0x0080FFFF, 0x00C0FFFF, 0x00E0FFFF, 0x00F0FFFF, 0x00F8FFFF, 0x00FCFFFF, 0x00FEFFFF,
	0x00FFFFFF, 0x80FFFFFF, 0xC0FFFFFF, 0xE0FFFFFF, 0xF0FFFFFF, 0xF8FFFFFF, 0xFCFFFFFF, 0xFEFFFFFF,
	0xFFFFFFFF
};

// All 128 possible IPv6 prefix masks for quick access.
static struct in6_addr all_ipv6_subnet_masks[128];

// The JSON parsing context structure.
struct subnet_activity_spec_context
{
	enum
	{
		INIT,
		SUBNET_ACTIVITY_SPEC,
		SUBNET_ACTIVITY_SPEC_EXPECT_MAPPING_ARRAY,
		SUBNET_MAPPING_ARRAY,
		SUBNET_MAPPING,
		SUBNET_MAPPING_EXPECT_ENTITY,
		SUBNET_MAPPING_EXPECT_PREFIXES_ARRAY,
		PREFIX_MAPPING_ARRAY,
		PREFIX_MAPPING,
		PREFIX_MAPPING_EXPECT_PREFIX,
		PREFIX_MAPPING_EXPECT_LENGTH
	} state;

	struct subnet_activity* subnet_metadata;
	enum subnet_activity_error sa_status_code;

	// Current processing state variables.
	struct prefix_match* current_prefix;
	struct entity* current_entity;
	struct in_addr46 current_ipaddr;
};

// Generates all possible IPv6 prefix masks and stores them in dest_mem.
static void generate_all_ipv6_prefixes(struct in6_addr* dest_mem)
{
	// Set constants for each unsigned char.
	const unsigned char onecharset[] = { 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF };

	// Generate 128 addresses.
	for (unsigned int i = 1; i <= 128; ++i)
	{
		// Zero out the whole IPv6 address initially.
		memset(&dest_mem[i - 1], 0, sizeof(struct in6_addr));

		const unsigned int div = i / 8;
		const unsigned int mod = i % 8;

		// Fill as many bytes are necessary.
		for (unsigned int j = 0; j < div; ++j)
		{
			dest_mem[i - 1].s6_addr[j] = onecharset[7];
		}

		// Set the remainder of bits less than 8.
		if (mod > 0)
		{
			dest_mem[i - 1].s6_addr[div] = onecharset[mod - 1];
		}
	}
}

// Adds a prefix to the hash table if it does not already exist.
static enum subnet_activity_error add_prefix_hh(struct prefix_match** ht, struct in_addr46* addr, const unsigned int plen, struct entity* p_entity, struct prefix_match** current_p_prefix)
{
	// Try to find whether the entry already exists.
	struct prefix_match* entry = NULL;
	struct prefix p;
	memset(&p, 0, sizeof(struct prefix));
	memcpy(&p.address, addr, sizeof(struct in_addr46));
	p.length = plen;
	HASH_FIND(hh, *ht, &p, sizeof(struct prefix), entry);

	// Add the prefix with the entity to the hash table.
	if (entry == NULL)
	{
		entry = (struct prefix_match*)calloc(1, sizeof(struct prefix_match));
		if (!entry)
		{
			return SA_ALLOC_FAILED;
		}

		memcpy(&entry->prefix, &p, sizeof(struct prefix));
		entry->associated_entity = p_entity;
		HASH_ADD(hh, *ht, prefix, sizeof(struct prefix), entry);
		*current_p_prefix = entry;
	}

	return SA_OK;
}

// Reads an integer from the JSON file.
static int subnet_activity_spec_integer(struct subnet_activity_spec_context* ctx, long long integerVal)
{
	switch (ctx->state)
	{
		case PREFIX_MAPPING_EXPECT_LENGTH:
			// Check whether the prefix length is valid.
			if ((ctx->current_ipaddr.af == AF_INET && integerVal >= 0 && integerVal <= 32) ||
				(ctx->current_ipaddr.af == AF_INET6 && integerVal >= 0 && integerVal <= 128))
			{
				// Add the prefix to internal storage.
				if ((ctx->sa_status_code = add_prefix_hh(&ctx->subnet_metadata->prefixes, &ctx->current_ipaddr, (unsigned int)integerVal, ctx->current_entity
					, &ctx->current_prefix)) == SA_OK)
				{
					++ctx->subnet_metadata->registered_prefixes;

					// Set prefix length bound variables.
					if (ctx->current_ipaddr.af == AF_INET)
					{
						ctx->subnet_metadata->shortest_ipv4_prefix = min(ctx->subnet_metadata->shortest_ipv4_prefix, (unsigned int)integerVal);
						ctx->subnet_metadata->longest_ipv4_prefix = max(ctx->subnet_metadata->longest_ipv4_prefix, (unsigned int)integerVal);
					}
					else
					{
						ctx->subnet_metadata->shortest_ipv6_prefix = min(ctx->subnet_metadata->shortest_ipv6_prefix, (unsigned int)integerVal);
						ctx->subnet_metadata->longest_ipv6_prefix = max(ctx->subnet_metadata->longest_ipv6_prefix, (unsigned int)integerVal);
					}
				}

				ctx->state = PREFIX_MAPPING;
				return 1;
			}
			else
			{
				ctx->sa_status_code = SA_INVALID_PREFIX_LENGTH;
				return 0;
			}
		default:
			ctx->sa_status_code = SA_JSON_SPEC_INTEGER_FAILED;
			return 0;
	}
}

// Reads a string from the JSON file.
static int subnet_activity_spec_string(struct subnet_activity_spec_context* ctx, const unsigned char* stringVal, size_t stringLen)
{
	switch (ctx->state)
	{
		case SUBNET_MAPPING_EXPECT_ENTITY:
			// Parse the entity name.
			strncpy(ctx->subnet_metadata->entities[ctx->subnet_metadata->registered_entities - 1]->name, (char*)stringVal, stringLen);
			ctx->state = SUBNET_MAPPING;
			return 1;
		default:
			ctx->sa_status_code = SA_JSON_SPEC_STRING_FAILED;
			return 0;
	}
}

// Performs key-value mapping for the JSON parsing process.
static int subnet_activity_spec_map_key(struct subnet_activity_spec_context* ctx, const unsigned char* stringVal, size_t stringLen)
{
	switch (ctx->state)
	{
		case SUBNET_ACTIVITY_SPEC:
			// We first expect a "subnet_activity" key in the JSON file.
			if (stringLen == 15 && memcmp(stringVal, "subnet_activity", 15) == 0)
			{
				ctx->state = SUBNET_ACTIVITY_SPEC_EXPECT_MAPPING_ARRAY;
			}
			return 1;
		case SUBNET_MAPPING:
			// Parse the subnet mapping, consisting of an entity name and an array of prefixes.
			if (stringLen == 6 && memcmp(stringVal, "entity", 6) == 0)
			{
				ctx->state = SUBNET_MAPPING_EXPECT_ENTITY;
			}
			else if (stringLen == 8 && memcmp(stringVal, "prefixes", 8) == 0)
			{
				ctx->state = SUBNET_MAPPING_EXPECT_PREFIXES_ARRAY;
			}
			return 1;
		case PREFIX_MAPPING:
			{
				// Parse the prefix mapping, consisting of an IPv4/IPv6 prefix and its length.
				// We first try to parse the IP address as IPv4.
				memset(&ctx->current_ipaddr, 0, sizeof(struct in_addr46));
				char ipv46[128] = { 0 };
				strncpy(ipv46, (char*)stringVal, stringLen);
				if (inet_pton(AF_INET, ipv46, &ctx->current_ipaddr.in.addr4) == 1)
				{
					// Succeeded parsing as IPv4.
					ctx->current_ipaddr.af = AF_INET;
				}
				// If it failed, we try to parse it as IPv6.
				else if (inet_pton(AF_INET6, ipv46, &ctx->current_ipaddr.in.addr6) == 1)
				{
					// Succeeded parsing as IPv6.
					ctx->current_ipaddr.af = AF_INET6;
				}
				else
				{
					// Failed to parse the IP-address.
					ctx->sa_status_code = SA_IPADDRESS_PARSE_FAILED;
					return 0;
				}

				ctx->state = PREFIX_MAPPING_EXPECT_LENGTH;
			return 1;
			}
		default:
			ctx->sa_status_code = SA_JSON_MAP_KEY_FAILED;
			return 0;
	}
}

// Initializes the JSON parsing process.
static int subnet_activity_spec_start_map(struct subnet_activity_spec_context* ctx)
{
	switch (ctx->state)
	{
		case INIT:
			ctx->state = SUBNET_ACTIVITY_SPEC;
			return 1;
		case SUBNET_MAPPING_ARRAY:
			// Initialize internal storage for the subnet mapping.
			ctx->subnet_metadata->entities[ctx->subnet_metadata->registered_entities] = calloc(1, sizeof(struct entity));
			ctx->current_entity = ctx->subnet_metadata->entities[ctx->subnet_metadata->registered_entities];
			if (!ctx->subnet_metadata->entities[ctx->subnet_metadata->registered_entities])
			{
				ctx->sa_status_code = SA_ALLOC_FAILED;
				return 0;
			}
			++ctx->subnet_metadata->registered_entities;
			ctx->state = SUBNET_MAPPING;
			return 1;
		case PREFIX_MAPPING_ARRAY:
			ctx->state = PREFIX_MAPPING;
			return 1;
		default:
			ctx->sa_status_code = SA_JSON_MAP_FAILED;
			return 0;
	}
}

// Ends the JSON parsing process.
static int subnet_activity_spec_end_map(struct subnet_activity_spec_context* ctx)
{
	switch (ctx->state)
	{
		case SUBNET_ACTIVITY_SPEC:
			ctx->state = INIT;
			return 1;
		case SUBNET_MAPPING:
			ctx->state = SUBNET_MAPPING_ARRAY;
			return 1;
		case PREFIX_MAPPING:
			// Increment the number of entities currently stored.
			ctx->state = PREFIX_MAPPING_ARRAY;
			return 1;
		default:
			ctx->sa_status_code = SA_JSON_END_MAP_FAILED;
			return 0;
	}
}

// Reads an array from the JSON file.
static int subnet_activity_spec_start_array(struct subnet_activity_spec_context* ctx)
{
	switch (ctx->state)
	{
		case SUBNET_ACTIVITY_SPEC_EXPECT_MAPPING_ARRAY:
			// Parse the subnet mapping array.
			ctx->state = SUBNET_MAPPING_ARRAY;
			return 1;
		case SUBNET_MAPPING_EXPECT_PREFIXES_ARRAY:
			// Parse the prefix mapping array.
			ctx->state = PREFIX_MAPPING_ARRAY;
			return 1;
		default:
			ctx->sa_status_code = SA_JSON_ARRAY_FAILED;
			return 0;
	}
}

// Ends an array from the JSON file.
static int subnet_activity_spec_end_array(struct subnet_activity_spec_context* ctx)
{

	switch (ctx->state)
	{
		case SUBNET_MAPPING_ARRAY:
			// End the array parsing process.
			ctx->state = SUBNET_ACTIVITY_SPEC;
			return 1;
		case PREFIX_MAPPING_ARRAY:
			ctx->state = SUBNET_MAPPING;
			return 1;
		default:
			ctx->sa_status_code = SA_JSON_END_ARRAY_FAILED;
			return 0;
	}
}

// Specifies the YAJL callbacks for parsing the JSON file of subnet mappings.
static yajl_callbacks subnet_activity_json_callbacks =
{
	NULL,
	NULL,
	(int (*)(void* ctx, long long int v))subnet_activity_spec_integer,
	NULL,
	NULL,
	(int (*)(void* ctx, const unsigned char* s, size_t l))subnet_activity_spec_string,
	(int (*)(void* ctx))subnet_activity_spec_start_map,
	(int (*)(void* ctx, const unsigned char* s, size_t l))subnet_activity_spec_map_key,
	(int (*)(void* ctx))subnet_activity_spec_end_map,
	(int (*)(void* ctx))subnet_activity_spec_start_array,
	(int (*)(void* ctx))subnet_activity_spec_end_array
};

// Loads the subnet activity file and initializes the required logic.
const enum subnet_activity_error subnet_activity_initialize(const char* subnetfile, struct subnet_activity* p_subact)
{
	// Initialize a context structure.
	struct subnet_activity_spec_context ctx =
	{
		.state = INIT,
		.subnet_metadata = p_subact,
		.sa_status_code = SA_OK,
	};

	// Initialize everything to zero.
	memset(p_subact, 0, sizeof(struct subnet_activity));

	// Set minimum values to the maximum, so that they will lower.
	p_subact->shortest_ipv4_prefix = 32;
	p_subact->shortest_ipv6_prefix = 128;

	// Try to open the subnet definition file.
	FILE* job_fh = NULL;
	if (subnetfile && p_subact)
	{
		job_fh = fopen(subnetfile, "r");
		if (!job_fh)
		{
			return SA_OPEN_FILE_FAILED;
		}
	}
	else
	{
		return SA_INVALID_ARGUMENT;
	}

	// Initialize the JSON parser.
	yajl_handle yajl = yajl_alloc(&subnet_activity_json_callbacks, NULL, &ctx);
	if (!yajl)
	{
		// Failed to initialize the JSON parser!
		return false;
	}

	yajl_status status = yajl_status_ok;
	uint8_t buf[READ_BLOCK_SIZE + 1];
	ssize_t rd;
	while (1)
	{
		// Read from the subnet definition file.
		rd = fread(buf, 1, READ_BLOCK_SIZE, job_fh);
		if (rd == 0 && feof(job_fh))
		{
			break;
		}

		// Parse the JSON data in the file.
		buf[rd] = 0;
		status = yajl_parse(yajl, buf, rd);
		if (status != yajl_status_ok)
		{
			status = yajl_complete_parse(yajl);
		}

		// Handle errors if required.
		if (status != yajl_status_ok)
		{
			break;
		}
	}

	// Free used resources and close subnet definition file.
	yajl_free(yajl);
	if (job_fh)
	{
		fclose(job_fh);
	}

	// Generate a lookup table for all possible IPv6 subnet masks, allowing fast prefix matching.
	generate_all_ipv6_prefixes(all_ipv6_subnet_masks);

	return ctx.sa_status_code;
}

// Tests an IP-address against all registered prefixes. The longest matching prefix is returned, including the associated entity.
const enum subnet_activity_error subnet_activity_match_prefix(const struct in_addr46* addr, struct subnet_activity* p_subact, struct prefix_match** const pp_result)
{
	// Check if the input parameters are valid.
	if (!p_subact || !addr || !pp_result)
	{
		return SA_INVALID_ARGUMENT;
	}

	// Check whether the address family is valid.
	if (addr->af != AF_INET && addr->af != AF_INET6)
	{
		return SA_INVALID_ARGUMENT;
	}

	// Set the output prefix_match pointer to NULL to mitigate caller mistakes.
	*pp_result = NULL;

	// Generate matching prefix combinations from long to short until the longest is found.
	// If none is found after all possible prefixes are generated, no matching prefix is
	// registered.
	struct prefix p;
	memset(&p, 0, sizeof(struct prefix));
	struct prefix_match* entry = NULL;
	if (addr->af == AF_INET)
	{
		p.address.af = AF_INET;
		for (unsigned int i = p_subact->longest_ipv4_prefix; i >= p_subact->shortest_ipv4_prefix; i--)
		{
			// Check whether prefix with length i is present in the hash table.
			p.address.in.addr4.s_addr = addr->in.addr4.s_addr & all_ipv4_subnet_masks[i];
			p.length = i;

			// Find prefix by hash.
			HASH_FIND(hh, p_subact->prefixes, &p, sizeof(struct prefix), entry);
			if (entry)
			{
				*pp_result = entry;
				break;
			}
		}
	}
	else
	{
		p.address.af = AF_INET6;
		for (unsigned int i = p_subact->longest_ipv6_prefix; i >= p_subact->shortest_ipv6_prefix; i--)
		{
			// Perform bitwise AND for all 16 bytes of the IPv6 address.
			for (unsigned int j = 0; j < 16; ++j)
			{
				p.address.in.addr6.s6_addr[j] = addr->in.addr6.s6_addr[j] & all_ipv6_subnet_masks[i - 1].s6_addr[j];
			}

			// Check whether prefix with length i is present in the hash table.
			p.length = i;

			// Find prefix by hash.
			HASH_FIND(hh, p_subact->prefixes, &p, sizeof(struct prefix), entry);
			if (entry)
			{
				*pp_result = entry;
				break;
			}
		}
	}

	return SA_OK;
}

// Destroys and frees the subnet activity structure.
const enum subnet_activity_error subnet_activity_destroy(struct subnet_activity* p_subact)
{
	if (!p_subact)
	{
		return SA_INVALID_ARGUMENT;
	}

	if (p_subact->registered_entities > 0 && p_subact->entities)
	{
		// Free entities.
		for (unsigned int i = 0; i < p_subact->registered_entities; ++i)
		{
			free(p_subact->entities[i]);
			p_subact->entities[i] = NULL;
		}

		// Free the hash table with prefix-entity mappings.
		struct prefix_match* pf_it = NULL;
		struct prefix_match* pf_tmp = NULL;
		HASH_ITER(hh, p_subact->prefixes, pf_it, pf_tmp)
		{
			// Free the hash table entry.
			HASH_DEL(p_subact->prefixes, pf_it);
			free(pf_it);
		}

		// Set counters to zero.
		p_subact->registered_entities = 0;
		p_subact->registered_prefixes = 0;
		p_subact->shortest_ipv4_prefix = 0;
		p_subact->longest_ipv4_prefix = 0;
		p_subact->shortest_ipv6_prefix = 0;
		p_subact->longest_ipv6_prefix = 0;
	}

	return SA_OK;
}
