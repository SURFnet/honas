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
#include "defines.h"
#include "honas_state.h"
#include "includes.h"
#include "logging.h"

static char* get_version_string(uint32_t major_version, uint32_t minor_version)
{
	static char strbuf[22];
	sprintf(strbuf, "%u.%u", major_version, minor_version);
	return strbuf;
}

static char* get_timestamp_string(time_t ts)
{
	static char strbuf[] = "YYYY-MM-DDTHH:MM:SS";
	strftime(strbuf, sizeof(strbuf), "%Y-%m-%dT%H:%M:%S", localtime(&ts));
	return strbuf;
}

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

static void show_general_information(honas_state_t* state, FILE* out)
{
	fprintf(out, "\n## Version information ##\n\n");
	fprintf(out, "Node version      : %s\n", VERSION);
	fprintf(out, "State file version: %s\n", get_version_string(state->header->major_version, state->header->minor_version));

	fprintf(out, "\n## Period information ##\n\n");
	fprintf(out, "Period begin                  : %s\n", get_timestamp_string(state->header->period_begin));
	fprintf(out, "First request                 : %s\n", get_timestamp_string(state->header->first_request));
	fprintf(out, "Last request                  : %s\n", get_timestamp_string(state->header->last_request));
	fprintf(out, "Period end                    : %s\n", get_timestamp_string(state->header->period_end));
	fprintf(out, "Estimated number of clients   : %u\n", state->header->estimated_number_of_clients);
	fprintf(out, "Estimated number of host names: %u \n", state->header->estimated_number_of_host_names);
	fprintf(out, "Number of requests            : %" PRIu64 "\n", state->header->number_of_requests);

	fprintf(out, "\n## Filter configuration ##\n\n");
	fprintf(out, "Number of filters         : %u\n", state->header->number_of_filters);
	fprintf(out, "Number of filters per user: %u\n", state->header->number_of_filters_per_user);
	fprintf(out, "Number of hashes          : %u\n", state->header->number_of_hashes);
	fprintf(out, "Number of bits per filter : %u\n", state->header->number_of_bits_per_filter);
	fprintf(out, "Flatten threshold         : %u\n", state->header->flatten_threshold);

	fprintf(out, "\n## Filter information ##\n\n");
	uint32_t filter_size = state->header->number_of_bits_per_filter >> 3;
	for (uint32_t i = 0; i < state->header->number_of_filters; i++) {
		uint32_t bits_set = state->filter_bits_set[i];
		uint32_t est_nr_host_names = bloom_approx_count(filter_size, state->header->number_of_hashes, bits_set);
		fprintf(out, "%2u. Number of bits set: %10u (Estimated number of host names: %10u)\n", i + 1, bits_set, est_nr_host_names);

		// Calculate and print the fill rate of the Bloom filter and its actual false positive rate.
		const double fillrate = bloom_fill_rate(bits_set, state->header->number_of_bits_per_filter);
		fprintf(out, "    Fill Rate:        %.10f (False positive probability:   %.10f)\n"
			, fillrate, bloom_actual_fpr(fillrate, state->header->number_of_hashes));
	}
	fprintf(out, "\n");
}

static void show_usage(char* program_name, FILE* out)
{
	fprintf(out, "Usage: %s [<options>] <state-file>\n\n", program_name);
	fprintf(out, "Options:\n");
	fprintf(out, "  -h|--help           Show this message\n");
	fprintf(out, "  -q|--quiet          Be more quiet (can be used multiple times)\n");
	fprintf(out, "  -v|--verbose        Be more verbose (can be used multiple times)\n");
}

static const struct option long_options[] = {
	{ "help", no_argument, 0, 'h' },
	{ "quiet", no_argument, 0, 'q' },
	{ "verbose", no_argument, 0, 'v' },
	{ 0, 0, 0, 0 }
};

int main(int argc, char** argv)
{
	char* program_name = "honas-info";
	char* state_file = NULL;

	/* Parse command line arguments */
	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "h", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 0:
			fprintf(stderr, "Unimplemented option %s; Aborting!", long_options[option_index].name);
			return 1;

		case 'h':
			show_usage(program_name, stdout);
			return 0;

		case '?':
			show_usage(program_name, stderr);
			return 1;

		case 'q':
			log_set_min_log_level(log_get_min_log_level() - 1);
			break;

		case 'v':
			log_set_min_log_level(log_get_min_log_level() + 1);
			break;

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

	/* Load Honas state file */
	honas_state_t state = { 0 };
	if (honas_state_load(&state, state_file, true) == -1) {
		fprintf(stderr, "Error while loading state file '%s': %s!\n", state_file, strerror(errno));
		return 1;
	}

	/* Print honas state information */
	show_general_information(&state, stdout);

	/* Close all files and cleanup resources */
	honas_state_destroy(&state);
	log_destroy();
	return 0;
}
