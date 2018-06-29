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

#include "bitset.h"
#include "bloom.h"
#include "defines.h"
#include "honas_state.h"
#include "includes.h"
#include "json_printer.h"
#include "logging.h"
#include "utils.h"

#include <yajl/yajl_parse.h>

#define READ_BLOCK_SIZE 65536
#define SHA256_DIGEST_LENGTH 32

// Compute the fill rate of a Bloom filter, given s and m.
static const double bloom_fill_rate(const uint32_t s, const uint32_t m)
{
        return (double)s / (double)m;
}

// Compute the actual false positive rate of a Bloom filter, given s, m and k.
static const double bloom_actual_fpr(const double fr, const uint32_t k)
{
        return pow(fr, (double)k);
}

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

static char* get_version_string(uint32_t major_version, uint32_t minor_version)
{
	static char strbuf[22];
	sprintf(strbuf, "%u.%u", major_version, minor_version);
	return strbuf;
}

static void add_general_information(honas_state_t* state, json_printer_t* printer)
{
	/* Gather and search versions */
	json_printer_object_pair_string(printer, "node_version", VERSION);
	json_printer_object_pair_string(printer, "state_file_version", get_version_string(state->header->major_version, state->header->minor_version));

	/* Period information */
	json_printer_object_pair_uint64(printer, "period_begin", state->header->period_begin);
	json_printer_object_pair_uint64(printer, "first_request", state->header->first_request);
	json_printer_object_pair_uint64(printer, "last_request", state->header->last_request);
	json_printer_object_pair_uint64(printer, "period_end", state->header->period_end);
	json_printer_object_pair_uint32(printer, "estimated_number_of_clients", state->header->estimated_number_of_clients);
	json_printer_object_pair_uint32(printer, "estimated_number_of_host_names", state->header->estimated_number_of_host_names);
	json_printer_object_pair_uint32(printer, "number_of_requests", state->header->number_of_requests);

	/* Filter configuration */
	json_printer_object_pair_uint32(printer, "number_of_filters", state->header->number_of_filters);
	json_printer_object_pair_uint32(printer, "number_of_filters_per_user", state->header->number_of_filters_per_user);
	json_printer_object_pair_uint32(printer, "number_of_hashes", state->header->number_of_hashes);
	json_printer_object_pair_uint32(printer, "number_of_bits_per_filter", state->header->number_of_bits_per_filter);
	json_printer_object_pair_uint32(printer, "flatten_threshold", state->header->flatten_threshold);

	/* Filter information */
	uint32_t filter_size = state->header->number_of_bits_per_filter >> 3;
	json_printer_object_key(printer, "filters");
	json_printer_array_begin(printer);
	for (uint32_t i = 0; i < state->header->number_of_filters; i++) {
		json_printer_object_begin(printer);
		json_printer_object_pair_uint32(printer, "number_of_bits_set", state->filter_bits_set[i]);
		json_printer_object_pair_uint32(printer, "estimated_number_of_host_names", bloom_approx_count(filter_size, state->header->number_of_hashes, state->filter_bits_set[i]));

		// Calculate and print the actual false positive rate of this Bloom filter.
		char fprstr[64];
		const double act_fpr = bloom_actual_fpr(bloom_fill_rate(state->filter_bits_set[i], state->header->number_of_bits_per_filter), state->header->number_of_hashes);
		snprintf(fprstr, sizeof(fprstr), "%.10f", act_fpr);
		json_printer_object_pair_string(printer, "actual_false_positive_rate", fprstr);
		json_printer_object_end(printer);
	}
	json_printer_array_end(printer);
}

struct search_spec_context {
	enum {
		INIT,
		SEARCH_SPEC,
		SEARCH_SPEC_EXPECT_GROUPS_ARRAY,
		GROUPS_ARRAY,
		GROUP,
		GROUP_EXPECT_ID,
		GROUP_EXPECT_HOST_NAME_MAP,
		HOST_NAME_MAP,
		HOST_NAME_MAP_EXPECT_VALUE
	} state;

	honas_state_t* honas_state;
	bool flatten_results;

	json_printer_t printer;

	uint64_t group_id;
	bool group_has_results;
	bool group_all_host_names_found;
	bitset_t group_filters_hit;
	bitset_t host_name_filters_hit;

	char* host_name_key;
	size_t host_name_key_alloc;
	size_t host_name_key_len;
};

static int search_spec_integer(struct search_spec_context* ctx, long long integerVal)
{
	switch (ctx->state) {
	case GROUP_EXPECT_ID:
		ctx->group_id = integerVal;
		ctx->state = GROUP;
		return 1;

	default:
		log_msg(ERR, "Encountered unexpected json element in context '%d'", ctx->state);
		return 0;
	}
}

static int search_spec_string(struct search_spec_context* ctx, const unsigned char* stringVal, size_t stringLen)
{
	switch (ctx->state) {
	case HOST_NAME_MAP_EXPECT_VALUE:
		ctx->state = HOST_NAME_MAP;
		{ /* Process hostname */
			uint8_t bytes[stringLen/2];
			char debug_msg[1024];
			strncpy(debug_msg, (char*)stringVal, stringLen);
			log_msg(DEBUG, "Decoding hex hostname hash '%s', of length: %zu", debug_msg, stringLen);
			if (!decode_string_hex((char*)stringVal, stringLen, bytes, sizeof(bytes))) {
				log_msg(WARN, "Unable to hex decode hostname hash '%s'", stringVal);
				return 1;
			}

			/* Determine if and where to put possible filters hit information */
			bitset_t *filters_hit;
			if (!ctx->group_all_host_names_found) {
				filters_hit = NULL;
			} else if (ctx->group_has_results) {
				bitset_clear(&ctx->host_name_filters_hit);
				filters_hit = &ctx->host_name_filters_hit;
			} else {
				bitset_clear(&ctx->group_filters_hit);
				filters_hit = &ctx->group_filters_hit;
			}

			uint32_t hits = honas_state_check_host_name_lookups(ctx->honas_state, byte_slice_from_array(bytes), filters_hit);
			if (ctx->flatten_results)
				hits = hits < ctx->honas_state->header->number_of_filters_per_user ? 0 : 1;

			if (hits > 0) {
				if (!ctx->group_has_results) {
					/* First group results starts the group result output */
					json_printer_object_begin(&ctx->printer);
					json_printer_object_key(&ctx->printer, "hostnames");
					json_printer_object_begin(&ctx->printer);
					ctx->group_has_results = true;
				} else if (ctx->group_all_host_names_found) {
					bitset_bitwise_and(&ctx->group_filters_hit, filters_hit);
				}

				json_printer_object_pair_uint32(&ctx->printer, ctx->host_name_key, hits);
			} else {
				ctx->group_all_host_names_found = false;
			}
		}
		return 1;

	default:
		log_msg(ERR, "Encountered unexpected json element in context '%d'", ctx->state);
		return 0;
	}
}

static int search_spec_map_key(struct search_spec_context* ctx, const unsigned char* stringVal, size_t stringLen)
{
	switch (ctx->state) {
	case SEARCH_SPEC:
		if (stringLen == 6 && memcmp(stringVal, "groups", 6) == 0)
			ctx->state = SEARCH_SPEC_EXPECT_GROUPS_ARRAY;
		else
			log_msg(INFO, "Ignoring unknown key '%s' in root", stringVal);
		return 1;

	case GROUP:
		if (stringLen == 2 && memcmp(stringVal, "id", 2) == 0)
			ctx->state = GROUP_EXPECT_ID;
		else if (stringLen == 9 && memcmp(stringVal, "hostnames", 9) == 0)
			ctx->state = GROUP_EXPECT_HOST_NAME_MAP;
		else
			log_msg(INFO, "Ignoring unknown key '%s' in group", stringVal);
		return 1;

	case HOST_NAME_MAP:
		if (stringLen + 1 > ctx->host_name_key_alloc) {
			/* Reallocate memory to double the needed length */
			ctx->host_name_key_alloc = (stringLen + 1) * 2;
			ctx->host_name_key = realloc(ctx->host_name_key, ctx->host_name_key_alloc);
		}

		/* Remember the host name key for now */
		memcpy(ctx->host_name_key, stringVal, stringLen);
		ctx->host_name_key_len = stringLen;
		ctx->host_name_key[stringLen] = 0;

		ctx->state = HOST_NAME_MAP_EXPECT_VALUE;
		return 1;

	default:
		log_msg(ERR, "Encountered unexpected json element in context '%d'", ctx->state);
		return 0;
	}
}

static int search_spec_start_map(struct search_spec_context* ctx)
{
	switch (ctx->state) {
	case INIT:
		json_printer_object_begin(&ctx->printer);
		add_general_information(ctx->honas_state, &ctx->printer);

		ctx->state = SEARCH_SPEC;
		return 1;

	case GROUPS_ARRAY:
		/* Reset group info; But only generate output if there are host name hits */
		ctx->group_id = 0;
		ctx->group_has_results = false;
		ctx->group_all_host_names_found = true;

		ctx->state = GROUP;
		return 1;

	case GROUP_EXPECT_HOST_NAME_MAP:
		ctx->state = HOST_NAME_MAP;
		return 1;

	default:
		log_msg(ERR, "Encountered unexpected json element in context '%d'", ctx->state);
		return 0;
	}
}

static int search_spec_end_map(struct search_spec_context* ctx)
{
	switch (ctx->state) {
	case SEARCH_SPEC:
		json_printer_object_end(&ctx->printer);
		ctx->state = INIT;
		return 1;

	case GROUP:
		/* Check if group had host name hits */
		if (ctx->group_has_results) {
			json_printer_object_end(&ctx->printer);
			json_printer_object_pair_uint64(&ctx->printer, "id", ctx->group_id);
			uint32_t hits = bitset_popcount(&ctx->group_filters_hit);
			if (ctx->flatten_results)
				hits = hits < ctx->honas_state->header->number_of_filters_per_user ? 0 : 1;
			json_printer_object_pair_uint32(&ctx->printer, "hits_by_all_hostnames", ctx->group_all_host_names_found ? hits : 0);
			json_printer_object_end(&ctx->printer);
		}

		ctx->state = GROUPS_ARRAY;
		return 1;

	case HOST_NAME_MAP:
		ctx->state = GROUP;
		return 1;

	default:
		log_msg(ERR, "Encountered unexpected json element in context '%d'", ctx->state);
		return 0;
	}
}

static int search_spec_start_array(struct search_spec_context* ctx)
{
	switch (ctx->state) {
	case SEARCH_SPEC_EXPECT_GROUPS_ARRAY:
		json_printer_object_pair_boolean(&ctx->printer, "flattened_results", ctx->flatten_results);
		json_printer_object_key(&ctx->printer, "groups");
		json_printer_array_begin(&ctx->printer);
		ctx->state = GROUPS_ARRAY;
		return 1;

	default:
		log_msg(ERR, "Encountered unexpected json element in context '%d'", ctx->state);
		return 0;
	}
}

static int search_spec_end_array(struct search_spec_context* ctx)
{
	switch (ctx->state) {
	case GROUPS_ARRAY:
		json_printer_array_end(&ctx->printer);
		ctx->state = SEARCH_SPEC;
		return 1;

	default:
		log_msg(ERR, "Encountered unexpected json element in context '%d'", ctx->state);
		return 0;
	}
}

static yajl_callbacks search_spec_json_callbacks = {
	NULL,
	NULL,
	(int (*)(void* ctx, long long int v))search_spec_integer,
	NULL,
	NULL,
	(int (*)(void* ctx, const unsigned char* s, size_t l))search_spec_string,
	(int (*)(void* ctx))search_spec_start_map,
	(int (*)(void* ctx, const unsigned char* s, size_t l))search_spec_map_key,
	(int (*)(void* ctx))search_spec_end_map,
	(int (*)(void* ctx))search_spec_start_array,
	(int (*)(void* ctx))search_spec_end_array
};

static void perform_search_job(honas_state_t* honas_state, uint32_t flatten_threshold, FILE* job_fh, FILE* result_fh)
{
	struct search_spec_context ctx = {
		.state = INIT,
		.honas_state = honas_state,
		.flatten_results = honas_state->header->estimated_number_of_host_names < flatten_threshold,
		.host_name_key = NULL,
		.host_name_key_alloc = 0,
	};
	bitset_create(&ctx.group_filters_hit, honas_state->header->number_of_filters);
	bitset_create(&ctx.host_name_filters_hit, honas_state->header->number_of_filters);
	json_printer_begin(&ctx.printer, result_fh);

	yajl_handle yajl = yajl_alloc(&search_spec_json_callbacks, NULL, &ctx);
	log_passert(yajl != NULL, "Failed to initialize yajl parser");

	yajl_status status = yajl_status_ok;
	uint8_t buf[READ_BLOCK_SIZE + 1];
	ssize_t rd;
	while (1) {
		rd = fread(buf, 1, READ_BLOCK_SIZE, job_fh);
		if (rd == 0) {
			if (feof(job_fh))
				break;
			log_pfail("Error reading search job spec");
		}

		buf[rd] = 0;
		status = yajl_parse(yajl, buf, rd);
		if (status != yajl_status_ok)
			break;
	}
	if (status == yajl_status_ok)
		status = yajl_complete_parse(yajl);

	if (status != yajl_status_ok) {
		unsigned char* str = yajl_get_error(yajl, 1, buf, rd);
		log_msg(CRIT, "Error parsing search spec: %s", str);
		yajl_free_error(yajl, str);
		exit(1);
	}

	bitset_destroy(&ctx.group_filters_hit);
	bitset_destroy(&ctx.host_name_filters_hit);
	json_printer_end(&ctx.printer);
	yajl_free(yajl);
	if (ctx.host_name_key != NULL)
		free(ctx.host_name_key);
}

static void show_usage(char* program_name, FILE* out)
{
	fprintf(out, "Usage: %s [<options>] <state-file>\n\n", program_name);
	fprintf(out, "Options:\n");
	fprintf(out, "  -h|--help           Show this message\n");
	fprintf(out, "  -j|--job <file>     File containing the search job (default: stdin)\n");
	fprintf(out, "  -r|--result <file>  File to which the results will be saved (default: stdout)\n");
	fprintf(out, "  -f|--flatten-threshold <clients>\n");
	fprintf(out, "                      If fewer than this amount of clients have been seen then\n");
	fprintf(out, "                      flatten the results (default: never flatten)\n");
	fprintf(out, "  -q|--quiet          Be more quiet (can be used multiple times)\n");
	fprintf(out, "  -s|--syslog         Log messages to syslog\n");
	fprintf(out, "  -v|--verbose        Be more verbose (can be used multiple times)\n");
}

static const struct option long_options[] = {
	{ "help", no_argument, 0, 'h' },
	{ "job", required_argument, 0, 'j' },
	{ "result", required_argument, 0, 'r' },
	{ "flatten-threshold", required_argument, 0, 'f' },
	{ "quiet", no_argument, 0, 'q' },
	{ "syslog", no_argument, 0, 's' },
	{ "verbose", no_argument, 0, 'v' },
	{ 0, 0, 0, 0 }
};

int main(int argc, char** argv)
{
	char* program_name = "honas-search";
	char *state_file = NULL, *job_file = NULL, *result_file = NULL;
	uint32_t flatten_threshold = 0;

	/* Parse command line arguments */
	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "hj:r:f:", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 0:
			fprintf(stderr, "Unimplemented option %s; Aborting!", long_options[option_index].name);
			return 1;

		case 'h':
			show_usage(program_name, stdout);
			return 0;

		case 'j':
			job_file = optarg;
			break;

		case 'r':
			result_file = optarg;
			break;

		case 'f':
			if (!my_strtouint32(optarg, &flatten_threshold, NULL, 10)) {
				fprintf(stderr, "Invalid value for 'flatten-threshold': %s!\n", optarg);
				return 1;
			}
			break;

		case 'q':
			log_set_min_log_level(log_get_min_log_level() - 1);
			break;

		case 's':
			log_init_syslog(program_name, DEFAULT_LOG_FACILITY);
			break;

		case 'v':
			log_set_min_log_level(log_get_min_log_level() + 1);
			break;

		case '?':
			show_usage(program_name, stderr);
			return 1;

		default:
			fprintf(stderr, "Unimplemented option '%c'; Aborting!", c);
			return 1;
		}
	}
	if (optind < argc - 1) {
		fprintf(stderr, "Unsupported argument(s) supplied: %s!\n", argv[optind]);
		show_usage(program_name, stderr);
		return 1;
	} else if (optind == argc) {
		fprintf(stderr, "Required '<state-file>' argument missing!\n");
		return 1;
	} else {
		state_file = argv[optind];
	}

	log_msg(INFO, "%s (version %s)", program_name, VERSION);

	/* Load search job specification */
	FILE* job_fh = stdin;
	if (job_file != NULL) {
		job_fh = fopen(job_file, "r");
		log_passert(job_fh != NULL, "Unable to open search job file '%s'", job_file);
	}

	/* Load Honas state file */
	honas_state_t state = { 0 };
	log_passert(honas_state_load(&state, state_file, true) != -1, "Error while loading state file '%s'", state_file);

	/* Open result file */
	FILE* result_fh = stdout;
	if (result_file != NULL) {
		result_fh = fopen(result_file, "w");
		log_passert(result_fh != NULL, "Unable to open result file '%s'", result_file);
	}

	/* Generate result data */
	perform_search_job(&state, flatten_threshold, job_fh, result_fh);

	/* Close all files and cleanup resources */
	log_passert(fclose(result_fh) == 0, "Failed to close result file");
	log_passert(fclose(job_fh) == 0, "Failed to close job file");
	honas_state_destroy(&state);
	log_destroy();
	return 0;
}
