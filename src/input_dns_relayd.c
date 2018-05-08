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

#include "input_dns_relayd.h"

#include "delim_reader.h"
#include "logging.h"
#include "utils.h"

typedef struct {
	int input_fd;
	delim_reader_t* reader;
} input_dns_relayd_t;

static const uint16_t relevant_dns_types[] = {
	1, /* A */
	2, /* NS */
	15, /* MX */
	28, /* AAAA */
};
static const size_t relevant_dns_types_count = sizeof(relevant_dns_types) / sizeof(relevant_dns_types[0]);

/* Returns a pointer after the last valid host name byte. */
static char* check_host_name_chars(char* str)
{
	while ((
		*str == '.' || *str == '-'
		|| (*str >= 'a' && *str <= 'z')
		|| (*str >= '0' && *str <= '9')
		|| (*str >= 'A' && *str <= 'Z'))) {
		str++;
	}
	return str;
}

static bool input_dns_relayd_parse_line(input_dns_relayd_t* UNUSED(state), char* line, uint32_t linelen, struct in_addr46* client, uint8_t** host_name, size_t* host_name_length)
{
	/* Ignore empty and comment lines */
	if (line[0] == 0 || line[0] == '#')
		return false;
	assert(line[linelen] == 0);

	uint64_t timestamp;
	sa_in46 ip;
	uint16_t type;
	uint16_t class;
	bool found_ip = false, found_host_name = false;

	/* parse format: "<timestamp> <ip> <hostname>./<class>/<type>" */
	char *token, *saveptr = line, *endptr;
	for (int field = 0; saveptr != NULL; field++) {
		/* Use 'strchr' instead of 'strtok_r'; this makes a noticable difference on large datasets */
		token = saveptr;
		saveptr = strchr(saveptr, ' ');
		if (saveptr != NULL) {
			*saveptr = 0;
			saveptr++;
		}

		switch (field) {
		case 0:
			if (!my_strtouint64(token, &timestamp, NULL, 10) || timestamp < 946684800 /* 2000-01-01T00:00:00Z */) {
				log_msg(INFO, "Ignoring line with invalid timestamp: %s", token);
				return false;
			}
			break;

		case 1:
			ip.any.af = AF_UNSPEC;
			if (parse_ip(token, &ip, 0) != 0) {
				log_msg(INFO, "Ignoring line with invalid ip address: %s", token);
				return false;
			}
			switch (ip.any.af) {
			case AF_INET:
				client->af = AF_INET;
				client->in.addr4 = ip.ipv4.sin_addr;
				break;

			case AF_INET6:
				client->af = AF_INET6;
				client->in.addr6 = ip.ipv6.sin6_addr;
				break;

			default:
				log_msg(DEBUG, "Unsupport client address family: %hu", ip.any.af);
				return false;
			}
			found_ip = true;
			break;

		case 2:
			/* Skip past valid host name characters */
			endptr = check_host_name_chars(token);
			if (endptr == token || *endptr != '/') {
				log_msg(INFO, "Ignoring line with invalid host name field: %s", token);
				return false;
			}
			*host_name = (uint8_t*)token;
			*host_name_length = endptr - token;

			/* Parse type */
			if (!my_strtouint16(++endptr, &type, &endptr, 10) || *endptr != '/') {
				log_msg(INFO, "Ignoring line with invalid request type: %s", token);
				return false;
			}

			/* Parse class */
			if (!my_strtouint16(++endptr, &class, NULL, 10)) {
				log_msg(INFO, "Ignoring line with invalid request class: %s", token);
				return false;
			}

			/* Make sure the value in 'host_name' is a zero-terminated string with only the host name */
			(*host_name)[*host_name_length] = '\0';

			/* Check request class */
			if (class != 1) {
				log_msg(DEBUG, "Ignoring DNS request for class '%#4.4hx': %s", class, *host_name);
				return false;
			}

			/* Check request type */
			bool relevant_dns_type = false;
			for (size_t i = 0; i < relevant_dns_types_count; i++) {
				if (type == relevant_dns_types[i]) {
					relevant_dns_type = true;
					break;
				}
			}
			if (!relevant_dns_type) {
				log_msg(DEBUG, "Ignoring DNS request for type '%#4.4hx': %s", type, *host_name);
				return false;
			}

			found_host_name = true;
			break;

		default:
			log_msg(INFO, "Unexpected field %d: %s", field, token);
		}
	}

	/* Return unless all conditions have been met */
	if (!(found_ip && found_host_name))
		return false;

	log_msg(DEBUG, "Parsed relevant dns request: ip: %s; host_name: %s", str_addr(&ip), *host_name);
	return true;
}

static void input_dns_relayd_destroy(input_dns_relayd_t* state)
{
	if (state->reader != NULL) {
		delim_reader_destroy(state->reader);
		free(state->reader);
		state->reader = NULL;
	}
	if (state->input_fd != fileno(stdin)) {
		if (close(state->input_fd) == -1)
			log_perror(ERR, "Close input_dns_relayd input");
		state->input_fd = 0;
	}
	free(state);
}

static int input_dns_relayd_init(input_dns_relayd_t** state_ptr)
{
	int saved_errno;
	int err_result = -1;

	input_dns_relayd_t* state = *state_ptr = (input_dns_relayd_t*)calloc(1, sizeof(input_dns_relayd_t));
	if (state == NULL)
		goto err_out;

	state->input_fd = fileno(stdin);
	return 0;

err_out:
	saved_errno = errno;
	if (state != NULL) {
		input_dns_relayd_destroy(state);
		*state_ptr = NULL;
	}
	errno = saved_errno;
	return err_result;
}

static int input_dns_relayd_parse_config_item(const char* UNUSED(filename), input_dns_relayd_t* UNUSED(state), unsigned int UNUSED(lineno), char* UNUSED(keyword), char* UNUSED(value), unsigned int UNUSED(length))
{
	return 0;
}

static void input_dns_relayd_finalize_config(input_dns_relayd_t* state)
{
	state->reader = malloc(sizeof(*state->reader));
	log_passert(state->reader != NULL, "Failed to allocate for input reader");
	log_passert(delim_reader_init(state->reader, state->input_fd, '\n', 8192) != -1, "Failed to initialize input reader");
}

static ssize_t input_dns_relayd_next(input_dns_relayd_t* state, struct in_addr46* client, uint8_t** host_name)
{
	size_t host_name_length = 0;

	/* Read lines from input file or stream */
	while (1) {
		char* rdata = NULL;
		ssize_t rlen = delim_reader_next(state->reader, &rdata);
		switch (rlen) {
		case -2:
			/* Failed to find line in the input buffer, print warning and try to continue */
			log_msg(WARN, "Failed to find item in input buffer for input_dns_relayd");
			break;

		case -1:
			/* Some errror (possibly EINTR or EAGAIN), let the caller handle this */
			return -1;

		case 0:
			/* Reached end of file */
			return 0;

		default:
			/* Found line in input buffer, process it */
			rdata[rlen - 1] = 0; /* Replace the separator with a nul-byte */
			if (input_dns_relayd_parse_line(state, rdata, rlen - 1, client, host_name, &host_name_length))
				return host_name_length;
		}
	}
}

honas_input_t input_dns_relayd = {
	"dns-relayd",
	(input_init_fn_t*)input_dns_relayd_init,
	(parse_item_t*)input_dns_relayd_parse_config_item,
	(input_finalize_config_fn_t*)input_dns_relayd_finalize_config,
	(input_next_fn_t*)input_dns_relayd_next,
	(input_destroy_fn_t*)input_dns_relayd_destroy,
};
