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

#include "bloom.h"
#include "defines.h"
#include "honas_state.h"
#include "includes.h"
#include "logging.h"

static void show_usage(char* program_name, FILE* out)
{
	// At most 32 state files.
	fprintf(out, "Usage: %s [<options>] <dst-state-file> <src-state-file>\n\n", program_name);
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
	char* program_name = "honas-combine";
	char* dst_state_filename = NULL;
	char* src_state_filename = NULL;
	honas_state_t dst_state = { 0 };
	honas_state_t src_state = { 0 };

	log_msg(INFO, "%s (version %s)", program_name, VERSION);

	/* Parse command line arguments */
	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "hvq", long_options, &option_index);
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

	// Provide an error when the state files are missing.
	if (optind == argc)
	{
		log_msg(ERR, "Required '<dst-state-file>' arguments missing!");
		return 1;
	}

	// We must have a destination and source state file to combine.
	if (argc - optind < 2)
	{
		log_msg(ERR, "A destination and source state file are required!");
		return 1;
	}

	// Store the state filenames.
	dst_state_filename = malloc(strlen(argv[optind]));
	src_state_filename = malloc(strlen(argv[optind + 1]));
	if (!dst_state_filename || !src_state_filename)
	{
		log_msg(ERR, "Failed to allocate memory for the state filenames.");
		return 1;
	}

	strcpy(dst_state_filename, argv[optind++]);
	strcpy(src_state_filename, argv[optind]);

	// Load destination state file.
	if (honas_state_load(&dst_state, dst_state_filename, false) == -1)
	{
		log_msg(ERR, "Error while loading state file '%s'!", dst_state_filename);
		free(dst_state_filename);
		free(src_state_filename);
		return 1;
	}
	else
	{
		log_msg(DEBUG, "Succesfully loaded state file '%s'!", dst_state_filename);
	}

	// Load source state file.
	if (honas_state_load(&src_state, src_state_filename, true) == -1)
	{
		log_msg(ERR, "Error while loading state file '%s'!", src_state_filename);
		free(dst_state_filename);
		free(src_state_filename);
		honas_state_destroy(&dst_state);
		return 1;
	}
	else
	{
		log_msg(DEBUG, "Succesfully loaded state file '%s'!", src_state_filename);
	}

	// Aggregate data from both the target and source states.
	if (honas_state_aggregate_combine(&dst_state, &src_state))
	{
		log_msg(INFO, "Aggregated states '%s' and '%s'!", dst_state_filename, src_state_filename);
	}
	else
	{
		log_msg(ERR, "Failed to aggregate states '%s' and '%s'!", dst_state_filename, src_state_filename);
	}

	// Destroy the source state and free the filename.
	honas_state_destroy(&src_state);
	free(src_state_filename);
	src_state_filename = NULL;

	// Check if the destination filename exists, and unlink to allow writing.
	if (access(dst_state_filename, F_OK) != -1)
        {
		unlink(dst_state_filename);
	}

	// Persist and destroy the destination state, and free the filename.
	honas_state_persist(&dst_state, dst_state_filename, true);
	honas_state_destroy(&dst_state);
	free(dst_state_filename);
	dst_state_filename = NULL;

	// Clean up final resources.
	log_destroy();
	return 0;
}
