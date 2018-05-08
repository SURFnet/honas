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

#include "input_dns_socket.h"

#include "logging.h"
#include "utils.h"

#include <sys/socket.h>
#include <sys/un.h>

#define UNIX_SOCKET_PATH "/var/run/honas/honas.sock"
#define UNIX_SOCKET_MAX_BACKLOG 25

// The DNS query data structure that is passed via the Unix socket.
struct dns_query_socket_t
{
	struct in_addr46 ipaddress;
	char domainname[256];
	unsigned short domain_length;
	unsigned short dnsclass;
	unsigned short dnsrecordtype;
};

// The state structure for this input module.
typedef struct {
	int socket_fd;
	struct sockaddr_un addr;
} input_dns_socket_t;

// ---------------------------------------------------------------------------------

static struct dns_query_socket_t* query_buffer = NULL;
static unsigned int skipped = 0;
static unsigned int processed = 0;
static unsigned int error = 0;

// The query types we will save.
static const uint16_t relevant_dns_types[] = {
        1, /* A */
        2, /* NS */
        15, /* MX */
        28, /* AAAA */
};
static const size_t relevant_dns_types_count = sizeof(relevant_dns_types) / sizeof(relevant_dns_types[0]);

// ---------------------------------------------------------------------------------

static void input_dns_socket_destroy(input_dns_socket_t* state)
{
	// Close the UNIX socket.
	if (state->socket_fd)
	{
		if (close(state->socket_fd) == -1)
		{
			log_perror(ERR, "Failed to close input_dns_socket input");
		}
		state->socket_fd = 0;
	}

	// Unlink the socket filename.
	unlink(UNIX_SOCKET_PATH);

	// Free the data buffer if previously initialized.
	if (query_buffer)
	{
		free(query_buffer);
	}

	// Free the state if previously initialized.
	if (state)
	{
		free(state);
	}

	// Provide some statistics about processed DNS queries.
	char stats[512];
	snprintf(stats, 512, "Processed %u packets, skipped %u irrelevant ones, and %u resulted in an error.", processed, skipped, error);
	log_msg(INFO, stats);
}

static int input_dns_socket_init(input_dns_socket_t** state_ptr)
{
	int saved_errno;
	int err_result = -1;

	processed = 0;
	skipped = 0;
	error = 0;

	input_dns_socket_t* state = *state_ptr = (input_dns_socket_t*)calloc(1, sizeof(input_dns_socket_t));
	if (state == NULL)
		goto err_out;

	// If the Honas process was killed ungracefully, the socket file is still present.
	if (access(UNIX_SOCKET_PATH, F_OK) != -1)
	{
		log_msg(INFO, "Unlinking existing socket file...");
		unlink(UNIX_SOCKET_PATH);
	}

	// Create UNIX socket on which data can be continuously received.
	state->socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (state->socket_fd == -1)
	{
		log_perror(ERR, "Failed to create Unix socket!");
		goto err_out;
	}

	const int bufsize = 16777216; // 16MB
	if (setsockopt(state->socket_fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) != -1)
	{
		log_msg(INFO, "Succesfully increased socket receive buffer size!");
	}
	else
	{
		log_perror(ERR, "Failed to increase the socket receive buffer size!");
	}

	// Bind the socket to a file (see define on top of file for default). We already set
	// the first byte of the 'sun_path' to zero with calloc.
	state->addr.sun_family = AF_UNIX;
	strncpy(state->addr.sun_path, UNIX_SOCKET_PATH, sizeof(state->addr.sun_path) - 1);
	if (bind(state->socket_fd, (struct sockaddr*)&state->addr, sizeof(struct sockaddr_un)) == -1)
	{
		log_perror(ERR, "Failed to bind Unix socket!");
		goto err_out;
	}

	// Create a read buffer for input data.
	query_buffer = calloc(1, sizeof(struct dns_query_socket_t));
	if (!query_buffer)
	{
		log_perror(ERR, "Failed to allocate memory for the input data buffer!");
		goto err_out;
	}

	return 0;

err_out:
	saved_errno = errno;
	if (state != NULL) {
		input_dns_socket_destroy(state);
		*state_ptr = NULL;
	}
	errno = saved_errno;
	return err_result;
}

static int input_dns_socket_parse_config_item(const char* UNUSED(filename), input_dns_socket_t* UNUSED(state), unsigned int UNUSED(lineno), char* UNUSED(keyword), char* UNUSED(value), unsigned int UNUSED(length))
{
	return 0;
}

static void input_dns_socket_finalize_config(input_dns_socket_t* state)
{

}

static ssize_t input_dns_socket_next(input_dns_socket_t* state, struct in_addr46* client, uint8_t** host_name)
{
	ssize_t retval = -1;

	// Read data from the Unix socket.
	const ssize_t bytes_read = read(state->socket_fd, query_buffer, sizeof(struct dns_query_socket_t));
	if (bytes_read == -1 && errno != EINTR)
	{
		char error_msg[512];
		snprintf(error_msg, 512, "Failed to read data from socket, error code: %i\n", errno);
		log_perror(ERR, error_msg);
		++error;
	}
	else if (bytes_read >= sizeof(struct dns_query_socket_t))
	{
		// Check whether the DNS query type is relevant.
		int found = 0;
		for (size_t i = 0; i < relevant_dns_types_count; i++)
		{
			if (query_buffer->dnsrecordtype == relevant_dns_types[i])
			{
				found = 1;
				break;
			}
		}

		// Extract the data that is saved in the Bloom filter.
		if (found)
		{
			memcpy(client, &query_buffer->ipaddress, sizeof(struct in_addr46));
			*host_name = (uint8_t*)query_buffer->domainname;
			retval = query_buffer->domain_length;
		}
		else
		{
			// This DNS query should be skipped, but no error occured!
			retval = 0;
			++skipped;
		}
	}
	else
	{
		char error_msg[512];
		snprintf(error_msg, 512, "Read only %zu bytes from socket, less than required size!", bytes_read);
		log_perror(ERR, error_msg);
		++error;
	}

	++processed;

	// We only reach this case if we run into an error.
	return retval;
}

// Function table descriptor for the socket input module.
honas_input_t input_dns_socket = {
	"dns-socket",
	(input_init_fn_t*)input_dns_socket_init,
	(parse_item_t*)input_dns_socket_parse_config_item,
	(input_finalize_config_fn_t*)input_dns_socket_finalize_config,
	(input_next_fn_t*)input_dns_socket_next,
	(input_destroy_fn_t*)input_dns_socket_destroy,
};
