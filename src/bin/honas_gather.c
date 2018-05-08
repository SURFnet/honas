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

#include "config.h"
#include "defines.h"
#include "includes.h"
#include "logging.h"
#include "utils.h"

#include "honas_gather_config.h"
#include "honas_input.h"
#include "honas_state.h"
#include "input_dns_relayd.h"
#include "input_dns_socket.h"

const char active_state_file_name[] = "active_state";

// Configure new input modules here.
static const honas_input_t* input_modules[] = { &input_dns_relayd, &input_dns_socket };
#define input_modules_count (sizeof(input_modules) / sizeof(input_modules[0]))

static void* input_module_states[input_modules_count];
static volatile bool shutdown_pending = false;
static volatile bool check_current_state = true;

static void input_modules_init()
{
	for (size_t i = 0; i < input_modules_count; i++) {
		log_msg(INFO, "Initializing input module '%s'", input_modules[i]->name);
		input_module_states[i] = NULL;
		log_passert(input_modules[i]->init(&input_module_states[i]) == 0, "Failed to initialize input modules '%s'", input_modules[i]->name);
	}
}

static void input_module_activate(const char* input_name, const honas_input_t** input_module, void** input_state)
{
	/* Lookup selected input module */
	for (size_t i = 0; i < input_modules_count; i++) {
		if (strcmp(input_modules[i]->name, input_name) == 0) {
			*input_module = input_modules[i];
			*input_state = input_module_states[i];
			break;
		}
	}
	if (*input_module == NULL)
		log_die("Unsupported input modules '%s' configured", input_name);

	/* Finalize selected input module configuration */
	if ((*input_module)->finalize_config != NULL)
		(*input_module)->finalize_config(*input_state);
}

static int parse_config_item(const char* filename, honas_gather_config_t* config, unsigned int lineno, char* keyword, char* value, unsigned int length)
{
	int parsed = honas_gather_config_parse_item(filename, config, lineno, keyword, value, length);
	for (size_t i = 0; i < input_modules_count; i++) {
		if (input_modules[i]->parse_config_item != NULL)
			parsed |= input_modules[i]->parse_config_item(filename, input_module_states[i], lineno, keyword, value, length);
	}
	return parsed;
}

static void load_gather_config(honas_gather_config_t* config, int dirfd, const char* config_file)
{
	log_passert(fchdir(dirfd) != -1, "Failed to change to initial working directory");
	config_read(config_file, config, (parse_item_t*)parse_config_item);
	log_passert(chdir(config->bloomfilter_path) != -1, "Failed to change to honas state directory '%s'", config->bloomfilter_path);
}

static bool try_open_active_state(honas_state_t* state)
{
	int result = honas_state_load(state, active_state_file_name, false);
	switch (result) {
	case -1:
		if (errno == ENOENT)
			return false;
		log_pfail("Failed to load honas state");

	case 0:
		/* File loaded succesfully */
		if (unlink(active_state_file_name) == -1)
			log_perror(ERR, "Failed to unlink old dirty state file '%s'", active_state_file_name);

		log_msg(INFO, "Loaded existing honas state from '%s'", active_state_file_name);
		return true;

	case 1:
		log_die("File '%s' is not a valid honas state file", active_state_file_name);

	case 2:
		log_die("Honas state file '%s' contains errors", active_state_file_name);

	default:
		log_die("Opening honas state returned unsupported result code '%d'", result);
	}
}

static void create_state(honas_gather_config_t* config, honas_state_t* state, uint64_t period_begin)
{
	uint64_t period_end = period_begin - (period_begin % config->period_length) + config->period_length;
	int result = honas_state_create(
		state,
		config->number_of_filters,
		config->number_of_bits_per_filter,
		config->number_of_hashes,
		config->number_of_filters_per_user,
		config->flatten_threshold);
	log_passert(result == 0, "Failed to create honas state");

	state->header->period_begin = period_begin;
	state->header->period_end = period_end;
	assert(state->header->first_request == 0);
	assert(state->header->last_request == 0);
	assert(state->header->number_of_requests == 0);
	assert(state->header->estimated_number_of_clients == 0);
	assert(state->header->estimated_number_of_host_names == 0);

	log_msg(INFO, "Created new honas state");
}

static void close_state(honas_state_t* state)
{
	honas_state_persist(state, active_state_file_name, true);
	honas_state_destroy(state);

	log_msg(NOTICE, "Saved honas state to '%s'", active_state_file_name);
}

static void finalize_state(honas_state_t* state)
{
	char period_file_name[] = "XXXX-XX-XXTXX:XX:XX.hs";
	time_t period_end_time = state->header->period_end;
	struct tm period_end_ts;
	strftime(period_file_name, sizeof(period_file_name), "%FT%T.hs", gmtime_r(&period_end_time, &period_end_ts));

	honas_state_persist(state, period_file_name, false);
	honas_state_destroy(state);

	log_msg(NOTICE, "Saved honas state to '%s'", period_file_name);
}

static void exit_signal_handler(int UNUSED(signal))
{
	shutdown_pending = true;
}

static void recheck_signal_handler(int UNUSED(signal))
{
	check_current_state = true;
}

#define set_signal_handler(signal, handler)            \
	do {                                               \
		struct sigaction _sa = { 0 };                  \
		_sa.sa_handler = (handler);                    \
		if (sigaction((signal), &_sa, NULL) == -1) {   \
			log_perror(ERR, "sigaction(" #signal ")"); \
			exit(1);                                   \
		}                                              \
	} while (0)

static void show_usage(char* program_name, FILE* out)
{
	fprintf(out, "Usage: %s [--help] [--config <file>]\n\n", program_name);
	fprintf(out, "  -h|--help           Show this message\n");
	fprintf(out, "  -c|--config <file>  Load config from file instead of " DEFAULT_HONAS_GATHER_CONFIG_PATH "\n");
	fprintf(out, "  -q|--quiet          Be more quiet (can be used multiple times)\n");
	fprintf(out, "  -s|--syslog         Log messages to syslog\n");
	fprintf(out, "  -v|--verbose        Be more verbose (can be used multiple times)\n");
	fprintf(out, "  -f|--fork           Fork the process as daemon (syslog must be enabled)\n");
}

static const struct option long_options[] = {
	{ "help", no_argument, 0, 'h' },
	{ "config", required_argument, 0, 'c' },
	{ "quiet", no_argument, 0, 'q' },
	{ "syslog", no_argument, 0, 's' },
	{ "verbose", no_argument, 0, 'v' },
	{ "fork", no_argument, 0, 'f' },
	{ 0, 0, 0, 0 }
};

int main(int argc, char** argv)
{
	char* program_name = "honas-gather";
	char* config_file = DEFAULT_HONAS_GATHER_CONFIG_PATH;
	static honas_gather_config_t config = { 0 };
	int daemonize = 0;
	int syslogenabled = 0;

	/* Parse command line arguments */
	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "hc:qsvf", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 0:
			log_msg(CRIT, "Unimplemented option %s; Aborting!", long_options[option_index].name);
			return 1;

		case 'h':
			show_usage(program_name, stdout);
			return 0;

		case 'c':
			config_file = optarg;
			break;

		case 'q':
			log_set_min_log_level(log_get_min_log_level() - 1);
			break;

		case 's':
			log_init_syslog(program_name, DEFAULT_LOG_FACILITY);
			syslogenabled = 1;
			break;

		case 'v':
			log_set_min_log_level(log_get_min_log_level() + 1);
			break;

		case '?':
			show_usage(program_name, stderr);
			return 1;

		case 'f':
			daemonize = 1;
			break;

		default:
			log_msg(CRIT, "Unimplemented option '%c'; Aborting!", c);
			return 1;
		}
	}
	if (optind < argc) {
		log_msg(CRIT, "Unsupported argument supplied: %s!", argv[optind]);
		show_usage(program_name, stderr);
		return 1;
	}

	// Check if forking was requested.
	if (daemonize)
	{
		// Check if syslog is enabled, otherwise forking doesn't work.
		if (!syslogenabled)
		{
			log_msg(ERR, "Cannot fork if syslog is not enabled for logging!");
			return 1;
		}

		// Try to fork the process.
		pid_t process_id = fork();

		// Failed to fork the process!
		if (process_id < 0)
		{
			log_msg(ERR, "Failed to fork the gather process!");
			return 1;
		}
		// We have to kill the parent process.
		else if (process_id > 0)
		{
			log_msg(INFO, "PID of forked process is %d", process_id);
			return 0;
		}
	}

	log_msg(INFO, "%s (version %s)", program_name, VERSION);

	/* Setup signal handlers */
	set_signal_handler(SIGCHLD, SIG_IGN);
	set_signal_handler(SIGALRM, recheck_signal_handler);
	set_signal_handler(SIGHUP, recheck_signal_handler);
	set_signal_handler(SIGINT, exit_signal_handler);
	set_signal_handler(SIGTERM, exit_signal_handler);
	set_signal_handler(SIGQUIT, exit_signal_handler);

	/* Open current working directory for consistent relative config file loading */
	int init_dirfd = open(".", O_PATH | O_DIRECTORY | O_CLOEXEC);
	log_passert(init_dirfd != -1, "Failed to open initial working directory");

	/* Initialize and read configuration */
	honas_gather_config_init(&config);
	input_modules_init();
	load_gather_config(&config, init_dirfd, config_file);
	honas_gather_config_finalize(&config);

	/* Activate the configured input module*/
	const honas_input_t* input_module = NULL;
	void* input_state = NULL;
	input_module_activate(config.input_name, &input_module, &input_state);

	/* Open or create honas state for this period */
	honas_state_t current_state = { 0 };
	if (!try_open_active_state(&current_state)) {
		create_state(&config, &current_state, time(NULL));
	}

	/* Main processing loop */
	log_msg(NOTICE, "Begin processing");
	while (!shutdown_pending) {
		if (check_current_state) {
			uint64_t now = time(NULL);
			int64_t wait = current_state.header->period_end - now;
			if (wait <= 0) {
				/* Finalize current state, reload config and create new current state */
				finalize_state(&current_state);
				load_gather_config(&config, init_dirfd, config_file);
				create_state(&config, &current_state, now);
				continue;
			}

			/* Schedule alarm to recheck some time in the future.
			 * We recheck at least once per minute to handle possible clock changes.
			 */
			alarm(MIN(wait, 60U));
			check_current_state = false;
		}

		/* Process dns requests */
		struct in_addr46 client = { 0 };
		uint8_t* host_name = NULL;
		ssize_t result = input_module->next(input_state, &client, &host_name);
		switch (result) {
		case -1:
			if (errno == EAGAIN || errno == EINTR) {
				/* Input module encountered an EAGAIN or EINTR error (possibly due to SIGALRM)
				 *	Continue running main loop normally.. */
			} else {
				log_perror(ERR, "Unexpected error reading input");
				shutdown_pending = true;
			}
			break;

		case 0:
			/* End of input reached; Shutdown cleanly.. */
			//shutdown_pending = true;

			// Nothing useful was received by the input plugin this round, but that doesn't matter...
			break;

		default:
			log_msg(DEBUG, "Processing host name lookup for '%s' from client '%s'", host_name, str_in_addr(&client));
			honas_state_register_host_name_lookup(&current_state, time(NULL), &client, host_name, result);
		}
	}
	log_msg(NOTICE, "Done processing");

	/* Clean shutdown; persist active current state */
	close_state(&current_state);

	/* Destroy previously initialized data */
	for (size_t i = 0; i < input_modules_count; i++) {
		input_modules[i]->destroy(input_module_states[i]);
		input_module_states[i] = NULL;
	}
	honas_gather_config_destroy(&config);

	log_msg(NOTICE, "Exiting");
	log_destroy();
	return 0;
}
