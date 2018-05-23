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
	fprintf(out, "Usage: %s [<options>] <state-file-1> <...> <state-file-n>\n\n", program_name);
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
	char* state_files[32] = { 0 };
	unsigned int state_file_count = 0;
	honas_state_t states[32] = { 0 };

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

	// Provide an error when the state files are missing.
	if (optind == argc)
	{
		fprintf(stderr, "Required '<state-file-?>' arguments missing!\n");
		return 1;
	}

	// We must have at least two state files to combine.
	if (argc - optind < 2)
	{
		fprintf(stderr, "At least two state files are required!\n");
		return 1;
	}

	// Get all state filenames (at most 32).
	int i = 0;
	while (i < argc - optind)
	{
		state_files[i] = malloc(strlen(argv[i + optind]));
		strcpy(state_files[i], argv[i + optind]);
		++i;
	}
	state_file_count = i;

	log_msg(INFO, "%s (version %s)", program_name, VERSION);

	// Were any state files provided?
	if (state_file_count)
	{
		// Try to load all the state files.
		bool failed = false;
		for (unsigned int i = 0; i < state_file_count; ++i)
		{
			/* Load Honas state file */
			if (honas_state_load(&states[i], state_files[i], true) == -1)
			{
				fprintf(stderr, "Error while loading state file '%s': %s!\n", state_files[i], strerror(errno));
				failed = true;
				break;
			}
		}

		// If all state files were loaded correctly, we can proceed.
		if (!failed)
		{
			// Combine all filters with the 'target' one, which is the first argument.
			for (unsigned int i = 1; i < state_file_count; ++i)
			{
				// Aggregate data from both the target and source filters.
				if (honas_state_aggregate_combine(&states[0], &states[i]))
				{
					log_msg(INFO, "Aggregated states %s and %s!", state_files[0], state_files[i]);
				}
				else
				{
					log_msg(ERR, "Failed to aggregate states %s and %s!", state_files[0], state_files[i]);
					break;
				}

				// Destroy the merged state and free the filename.
				honas_state_destroy(&states[i]);
				free(state_files[i]);
				state_files[i] = NULL;
			}
		}
	}
	else
	{
		log_msg(ERR, "No state files were provided!");
	}

	// Finalize and persist the destination state.
	honas_state_persist(&states[0], state_files[0], true);
	honas_state_destroy(&states[0]);
	free(state_files[0]);
	state_files[0] = NULL;

	// Clean up final resources.
	log_destroy();
	return 0;
}
