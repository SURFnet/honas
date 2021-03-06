/*
 * Copyright (c) 2017, Quarantainenet Holding B.V.
 * Edited in 2018 by Gijs Rijnders, SURFnet
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
#include "honas_state.h"
#include "instrumentation.h"
#include "subnet_activity.h"
#include "advice.h"

#include <sys/un.h>

// Requires libevent2, libfstrm and ldns.
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <fstrm.h>
#include <ldns/ldns.h>

// Include protobuf compiled header.
#include <dnstap.pb-c.h>

#define UNIX_SOCKET_PATH	"/var/spool/honas/honas.sock"
#define CONTENT_TYPE		"protobuf:dnstap.Dnstap"
#define CAPTURE_HIGH_WATERMARK	262144
#define HONAS_INSTRUMENTATION	"/var/spool/honas/instrumentation.log"
#define HONAS_SUBNET_FILE	"/etc/honas/subnet_activity.json"
#define HONAS_DRYRUNFILE	"/var/spool/honas/dry_run.log"
#define FPR_THRESHOLD		0.001

static const char active_state_file_name[] = "active_state";
static honas_state_t current_active_state;
static volatile bool shutdown_pending = false;
static volatile bool check_current_state = true;
static honas_gather_config_t config = { 0 };
static int init_dirfd = 0;
static char* config_file = DEFAULT_HONAS_GATHER_CONFIG_PATH;
static FILE* inst_fd = NULL;
static FILE* dryrun_fd = NULL;

// Structure keeping track of the instrumentation information.
struct instrumentation* inst_data;

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

// --------------------------------------------------------------------------------------------------------

typedef enum
{
	CONN_STATE_READING_CONTROL_READY,
	CONN_STATE_READING_CONTROL_START,
	CONN_STATE_READING_DATA,
	CONN_STATE_STOPPED,
} conn_state;

// The Honas gather program context.
struct capture
{
        struct sockaddr_un              addr;
        struct event_base*              ev_base;
        struct evconnlistener*          ev_connlistener;
	struct event*			ev_inst_timer;
	int				remaining_connections;
	bool				aggregate_subnets;
	struct subnet_activity		subnet_metadata;
	bool				dry_run;
	struct event*			ev_dry_run_hourly;
	struct event*			ev_dry_run_daily;
	struct dry_run_counters		dry_run_data;
	bool				fpr_warning_passed;
	struct event*			ev_state_rotation;
};

// Global instance of capture context structure.
struct capture ctx;

// The context structure for a new connection.
struct connection
{
        struct capture*			context;
	conn_state			state;
	size_t				count_read;
	size_t				bytes_read;
	size_t				bytes_skip;
	size_t				len_buf;
	size_t				len_frame_payload;
	size_t				len_frame_total;
        struct bufferevent*             bev;
        struct evbuffer*                ev_input;
        struct evbuffer*                ev_output;
        struct fstrm_control*           control;
	struct fstrm_reader_options*	reader_options;
	struct fstrm_rdwr*		rdwr;
	struct fstrm_reader*		reader;
};

// The query types we will save.
static const uint16_t relevant_dns_types[] = {
        LDNS_RR_TYPE_A, /* A */
        LDNS_RR_TYPE_NS, /* NS */
        LDNS_RR_TYPE_MX, /* MX */
        LDNS_RR_TYPE_AAAA, /* AAAA */
	LDNS_RR_TYPE_PTR, /* PTR */
};
static const size_t relevant_dns_types_count = sizeof(relevant_dns_types) / sizeof(relevant_dns_types[0]);

// Checks whether we are dealing with a relevant DNS query.
static bool query_is_valid_dns_type(const ldns_rr_type qtype)
{
	for (int i = 0; i < relevant_dns_types_count; ++i)
	{
		if (qtype == relevant_dns_types[i])
		{
			return true;
		}
	}

	return false;
}

// --------------------------------------------------------------------------------------------------------

// Verifies that the FrameStream data is actually DNStap data.
static bool verify_content_type(struct fstrm_control* control, const uint8_t* content_type, const size_t len_content_type)
{
	fstrm_res res;
	size_t n_content_type = 0;
	const uint8_t *r_content_type = NULL;
	size_t len_r_content_type = 0;

	// Check the content type.
	res = fstrm_control_get_num_field_content_type(control, &n_content_type);
	if (res != fstrm_res_success)
	{
		return false;
	}

	if (n_content_type > 0)
	{
		res = fstrm_control_get_field_content_type(control, 0, &r_content_type, &len_r_content_type);
		if (res != fstrm_res_success)
			return false;

		if (len_content_type != len_r_content_type)
			return false;

		if (memcmp(content_type, r_content_type, len_content_type) == 0)
			return true;
	}

	return false;
}

// Initialized a newly accepted connection.
static struct connection* conn_init(struct capture* pctx)
{
	// Initialize connection properties.
        struct connection* conn = calloc(1, sizeof(struct connection));
        conn->context = pctx;
        conn->state = CONN_STATE_READING_CONTROL_READY;
        conn->control = fstrm_control_init();
	return conn;
}

// Destroys a connection object.
static void conn_destroy(struct connection** conn)
{
        if (*conn != NULL)
        {
                fstrm_control_destroy(&(*conn)->control);
                free(*conn);
        }
}

// Closes an accepted connection.
static void cb_close_conn(struct bufferevent* bev, short error, void *arg)
{
        struct connection* conn = (struct connection*)arg;
	struct capture* ctx = conn->context;

        if (error & BEV_EVENT_ERROR)
        {
                log_msg(ERR, "Closed client connection with error: ", strerror(errno), errno);
        }

        // Print statistics.
        log_msg(INFO, "Closing socket. Read %zd frames and %zd bytes).", conn->count_read, conn->bytes_read);

        /*
         * The BEV_OPT_CLOSE_ON_FREE flag is set on our bufferevent's, so the
         * following call to bufferevent_free() will close the underlying
         * socket transport.
         */
        bufferevent_free(bev);
        conn_destroy(&conn);

	++ctx->remaining_connections;
	if (ctx->remaining_connections == 1)
	{
		evconnlistener_enable(ctx->ev_connlistener);
	}
}

// Is there a full frame available for reading?
static bool can_read_full_frame(struct connection* conn)
{
	uint32_t tmp[2] = { 0 };

	/*
	 * This tracks the total number of bytes that must be removed from the
	 * input buffer to read the entire frame. */
	conn->len_frame_total = 0;

	/* Check if the frame length field has fully arrived. */
	if (conn->len_buf < sizeof(uint32_t))
		return false;

	/* Read the frame length field. */
	evbuffer_copyout(conn->ev_input, &tmp[0], sizeof(uint32_t));
	conn->len_frame_payload = ntohl(tmp[0]);

	/* Account for the frame length field. */
	conn->len_frame_total += sizeof(uint32_t);

	/* Account for the length of the frame payload. */
	conn->len_frame_total += conn->len_frame_payload;

	/* Check if this is a control frame. */
	if (conn->len_frame_payload == 0)
	{
		uint32_t len_control_frame = 0;

		/*
		 * Check if the control frame length field has fully arrived.
		 * Note that the input buffer hasn't been drained, so we also
		 * need to account for the initial frame length field. That is,
		 * there must be at least 8 bytes available in the buffer.
		 */
		if (conn->len_buf < 2*sizeof(uint32_t))
			return false;

		/* Read the control frame length. */
		evbuffer_copyout(conn->ev_input, &tmp[0], 2*sizeof(uint32_t));
		len_control_frame = ntohl(tmp[1]);

		/* Account for the length of the control frame length field. */
		conn->len_frame_total += sizeof(uint32_t);

		/* Enforce minimum and maximum control frame size. */
		if (len_control_frame < sizeof(uint32_t) ||
		    len_control_frame > FSTRM_CONTROL_FRAME_LENGTH_MAX)
		{
			cb_close_conn(conn->bev, 0, conn);
			return false;
		}

		/* Account for the control frame length. */
		conn->len_frame_total += len_control_frame;
	}

	/*
	 * Check if the frame has fully arrived. 'len_buf' must have at least
	 * the number of bytes needed in order to read the full frame, which is
	 * exactly 'len_frame_total'.
	 */
	if (conn->len_buf < conn->len_frame_total)
	{
		log_msg(DEBUG, "Incomplete message (have %zd bytes, want %u)",  conn->len_buf, conn->len_frame_total);
		if (conn->len_frame_total > CAPTURE_HIGH_WATERMARK)
		{
			log_msg(DEBUG, "Skipping %zu byte message (%zu buffer)!", conn->len_frame_total, CAPTURE_HIGH_WATERMARK);
			conn->bytes_skip = conn->len_frame_total;
		}
		return false;
	}

	/* Success. The entire frame can now be read from the buffer. */
	return true;
}

// Processes a DNStap message, and gives output to the Bloom filters.
static bool decode_dnstap_message(const Dnstap__Message* m)
{
	bool return_val = false;

	// Determine whether we are dealing with a DNS query, and we only want to process queries that have questions.
	if (m->type == DNSTAP__MESSAGE__TYPE__CLIENT_QUERY && m->has_query_message)
	{
		const ProtobufCBinaryData* message = &m->query_message;
		ldns_pkt* pkt = NULL;
		ldns_status status = ldns_wire2pkt(&pkt, message->data, message->len);
		if (status == LDNS_STATUS_OK)
		{
			// Retrieve the source IP-address of the query.
			struct in_addr46 client = { 0 };
			if (m->has_query_address)
			{
				if (m->query_address.len == 4)
				{
					// Store the IPv4 source address.
					client.af = AF_INET;
					memcpy(&client.in.addr4, m->query_address.data, sizeof(client.in.addr4));
				}
				else if (m->query_address.len == 16)
				{
					// Store the IPv6 source address.
					client.af = AF_INET6;
					memcpy(&client.in.addr6, m->query_address.data, sizeof(client.in.addr6));
				}
			}

			// Retrieve the resource record for the question.
			const ldns_rr* rr = ldns_rr_list_rr(ldns_pkt_question(pkt), 0);
			if (rr)
			{
				const ldns_rdf* qname = ldns_rr_owner(rr);
				const ldns_rr_class qclass = ldns_rr_get_class(rr);
				const ldns_rr_type qtype = ldns_rr_get_type(rr);

				// Does the question contain a domain name? Furthermore, we only process IN class queries.
				// Also, only queries for the A, NS, MX, AAAA record types are accepted.
				if (qname && qclass == LDNS_RR_CLASS_IN && query_is_valid_dns_type(qtype))
				{
					// Create a buffer for the hostname, including space for a possible entity name.
					char hn_buf[256] = { 0 };

					// Provide instrumentation data for subnet activity.
					size_t in = 0, notin = 0;

					// Check whether subnet aggregation was requested.
					if (ctx.aggregate_subnets)
					{
						// Look up the address in the prefix-entity mapping subsystem.
						struct prefix_match* match_ptr = NULL;
						if (subnet_activity_match_prefix(&client, &ctx.subnet_metadata, &match_ptr) == SA_OK && match_ptr)
						{
							strncpy(hn_buf, match_ptr->associated_entity->name, sizeof(match_ptr->associated_entity->name));
							in = 1;
						}
						else
						{
							notin = 1;
						}

						// Update instrumentation statistics.
						instrumentation_update_subnet_activity(inst_data, in, notin);
					}

					// Convert all DNS query data accordingly.
					char* hostname = ldns_rdf2str(qname);
					const size_t hostname_length = strlen(hostname);
					char* class_str = ldns_rr_class2str(qclass);
					char* type_str = ldns_rr_type2str(qtype);

					// Debug which domain names are stored.
					log_msg(DEBUG, "%s@%s stored in Bloom filter!", hn_buf, hostname);

					// Store the DNS query in the Bloom filters.
					honas_state_register_host_name_lookup(&current_active_state, time(NULL), &client, (uint8_t*)hostname, hostname_length
						, in == 1 ? (uint8_t*)hn_buf : (uint8_t*)"UNKNOWN", in == 1 ? strlen((char*)hn_buf) : strlen("UNKNOWN")
						, ctx.dry_run ? &ctx.dry_run_data : NULL, qtype);

					// Calculate the actual false positive rate, and check whether it is still acceptable.
					for (uint32_t i = 0; i < current_active_state.header->number_of_filters; i++)
					{
						const uint32_t bits_set = current_active_state.filter_bits_set[i];
						const double fill_rate = (double)bits_set / (double)current_active_state.header->number_of_bits_per_filter;
						const double act_fpr = pow(fill_rate, (double)current_active_state.header->number_of_hashes);

						// Does the false positive rate of this filter exceed the threshold?
						if (act_fpr > FPR_THRESHOLD && !ctx.fpr_warning_passed)
						{
							log_msg(WARN, "The actual false positive rate %f of filter %i exceeds the threshold %f!", act_fpr, i, FPR_THRESHOLD);
							ctx.fpr_warning_passed = true;
						}
					}

					// Update the instrumentation elements.
					instrumentation_increment_accepted(inst_data);
					instrumentation_increment_type(inst_data, qtype);

					// Free the converted values.
					free(type_str);
					free(class_str);
					free(hostname);
				}
			}

			// The packet was succesfully decoded, even if not processed.
			return_val = true;

		}
		else
		{
			instrumentation_increment_skipped(inst_data);
		}

		// Free the LDNS resources.
		ldns_pkt_free(pkt);
	}
	else
	{
		instrumentation_increment_skipped(inst_data);
	}

	instrumentation_increment_processed(inst_data);
	return return_val;
}

// Processes a data frame in the DNStap payload.
static void process_data_frame(struct connection* conn)
{
	/*
	 * Peek at 'conn->len_frame_total' bytes of data from the evbuffer, and
	 * write them to the output file.
	 */

	/* Determine how many iovec's we need to read. */
	const int n_vecs = evbuffer_peek(conn->ev_input, conn->len_frame_total, NULL, NULL, 0);

	/* Allocate space for the iovec's. */
	struct evbuffer_iovec vecs[n_vecs];

	/* Retrieve the iovec's. */
	const int n = evbuffer_peek(conn->ev_input, conn->len_frame_total, NULL, vecs, n_vecs);
	assert(n == n_vecs);

	// Find out what the total frame size is and read the data into a local buffer.
	size_t bytes_read = 0;
	uint8_t buf[4096];
	for (int i = 0; i < n_vecs; i++)
	{
		size_t len = vecs[i].iov_len;

		// Only read up to 'conn->len_frame_total' bytes.
		if (bytes_read + len > conn->len_frame_total)
		{
			len = conn->len_frame_total - bytes_read;
		}

		// Read the IOvec into a local buffer.
		memcpy(buf + bytes_read, vecs[i].iov_base, len);
		bytes_read += len;
	}

	/* Check that exactly the right number of bytes were written. */
	log_msg(DEBUG, "Read %zu bytes from the event buffer; total frame length is %zu.", bytes_read, conn->len_frame_total);
	assert(bytes_read == conn->len_frame_total);

	/* Delete the data frame from the input buffer. */
	evbuffer_drain(conn->ev_input, conn->len_frame_total);

	// Check whether the data frame actually has the correct content type.
	log_msg(DEBUG, "Verifying content type of data frame...");
	if (verify_content_type(conn->control, (const uint8_t*)CONTENT_TYPE, strlen(CONTENT_TYPE)))
	{
		log_msg(DEBUG, "Processing data frame of %zu bytes in size...", bytes_read);
	}
	else
	{
		log_msg(ERR, "Failed to process data frame: invalid content type!");
	}

	// Decode the frame as DNStap.
	Dnstap__Dnstap *d = dnstap__dnstap__unpack(NULL, bytes_read, buf);

	// Check if both the unpacked data is valid, and if the unpacked data
	// actually contains a valid message.
	if (d)
	{
		if (d->message)
		{
			// Try to decode the DNStap message.
			if (!decode_dnstap_message(d->message))
			{
				log_msg(ERR, "Failed to decode the DNStap message!");
				instrumentation_increment_invalid(inst_data);
			}
		}

		// Clean up protobuf allocated structures.
		dnstap__dnstap__free_unpacked(d, NULL);
	}
	else
	{
		log_msg(DEBUG, "Failed to unpack the frame into DNStap data!");
	}

	/* Accounting. */
	conn->count_read += 1;
	conn->bytes_read += bytes_read;
}

static bool match_content_type(struct connection* conn)
{
	fstrm_res res;

	/* Match the "Content Type" against ours. */
	res = fstrm_control_match_field_content_type(conn->control,
		(const uint8_t *) CONTENT_TYPE, strlen(CONTENT_TYPE));
	if (res != fstrm_res_success)
	{
		return false;
	}

	/* Success. */
	return true;
}

// Loads the protobuf control frame from the stream.
static bool load_control_frame(struct connection* conn)
{
	fstrm_res res;
	uint8_t *control_frame = NULL;

	/* Check if the frame is too big. */
	if (conn->len_frame_total >= FSTRM_CONTROL_FRAME_LENGTH_MAX)
	{
		/* Malformed. */
		return false;
	}

	/* Get a pointer to the full, linearized control frame. */
	control_frame = evbuffer_pullup(conn->ev_input, conn->len_frame_total);
	if (!control_frame)
	{
		/* Malformed. */
		return false;
	}
	log_msg(DEBUG, "Reading FrameStream control frame (%u bytes).", conn->len_frame_total);

	/* Decode the control frame. */
	res = fstrm_control_decode(conn->control,
				   control_frame,
				   conn->len_frame_total,
				   FSTRM_CONTROL_FLAG_WITH_HEADER);
	if (res != fstrm_res_success)
	{
		/* Malformed. */
		return false;
	}

	/* Drain the data read. */
	evbuffer_drain(conn->ev_input, conn->len_frame_total);

	/* Success. */
	return true;
}

// Sends a control frame to the bufferevent.
static bool send_frame(struct connection* conn, const void *data, size_t size)
{
	if (bufferevent_write(conn->bev, data, size) != 0)
	{
		log_msg(WARN, "Failed to write FrameStream control frame!");
		return false;
	}

	return true;
}

// Writes a control frame to the stream.
static bool write_control_frame(struct connection* conn)
{
	fstrm_res res;
	uint8_t control_frame[FSTRM_CONTROL_FRAME_LENGTH_MAX];
	size_t len_control_frame = sizeof(control_frame);

	/* Encode the control frame. */
	res = fstrm_control_encode(conn->control,
		control_frame, &len_control_frame,
		FSTRM_CONTROL_FLAG_WITH_HEADER);
	if (res != fstrm_res_success)
		return false;

	/* Send the control frame. */
	fstrm_control_type type = 0;
	(void)fstrm_control_get_type(conn->control, &type);
	if (!send_frame(conn, control_frame, len_control_frame))
		return false;

	/* Success. */
	return true;
}

static bool process_control_frame_ready(struct connection* conn)
{
	fstrm_res res;

	const uint8_t *content_type = NULL;
	size_t len_content_type = 0;
	size_t n_content_type = 0;

	/* Retrieve the number of "Content Type" fields. */
	res = fstrm_control_get_num_field_content_type(conn->control, &n_content_type);
	if (res != fstrm_res_success)
		return false;

	for (size_t i = 0; i < n_content_type; i++) {
		res = fstrm_control_get_field_content_type(conn->control, i,
							   &content_type,
							   &len_content_type);
		if (res != fstrm_res_success)
			return false;
	}

	/* Match the "Content Type" against ours. */
	if (!match_content_type(conn))
		return false;

	/* Setup the ACCEPT frame. */
	fstrm_control_reset(conn->control);
	res = fstrm_control_set_type(conn->control, FSTRM_CONTROL_ACCEPT);
	if (res != fstrm_res_success)
		return false;
	res = fstrm_control_add_field_content_type(conn->control,
		(const uint8_t *) CONTENT_TYPE, strlen(CONTENT_TYPE));
	if (res != fstrm_res_success)
		return false;

	/* Send the ACCEPT frame. */
	if (!write_control_frame(conn))
		return false;

	/* Success. */
	conn->state = CONN_STATE_READING_CONTROL_START;
	return true;
}

static bool process_control_frame_start(struct connection* conn)
{
	/* Match the "Content Type" against ours. */
	if (!match_content_type(conn))
		return false;

	/* Success. */
	conn->state = CONN_STATE_READING_DATA;
	return true;
}

static bool process_control_frame_stop(struct connection* conn)
{
	fstrm_res res;

	/* Setup the FINISH frame. */
	fstrm_control_reset(conn->control);
	res = fstrm_control_set_type(conn->control, FSTRM_CONTROL_FINISH);
	if (res != fstrm_res_success)
		return false;

	/* Send the FINISH frame. */
	if (!write_control_frame(conn))
		return false;

	conn->state = CONN_STATE_STOPPED;

	/* We return true here, which prevents the caller from closing
	 * the connection directly (with the FINISH frame still in our
	 * write buffer). The connection will be closed after the FINISH
	 * frame is written and the write callback (cb_write) is called
	 * to refill the write buffer.
	 */
	return true;
}

// Processes the control frame in the protobuf stream.
static bool process_control_frame(struct connection* conn)
{
	fstrm_res res;
	fstrm_control_type type;

	/* Get the control frame type. */
	res = fstrm_control_get_type(conn->control, &type);
	if (res != fstrm_res_success)
		return false;

	switch (conn->state) {
	case CONN_STATE_READING_CONTROL_READY: {
		if (type != FSTRM_CONTROL_READY)
			return false;
		return process_control_frame_ready(conn);
	}
	case CONN_STATE_READING_CONTROL_START: {
		if (type != FSTRM_CONTROL_START)
			return false;
		return process_control_frame_start(conn);
	}
	case CONN_STATE_READING_DATA: {
		if (type != FSTRM_CONTROL_STOP)
			return false;
		return process_control_frame_stop(conn);
	}
	default:
		return false;
	}

	/* Success. */
	return true;
}

// Callback for reading from DNStap socket.
static void cb_read(struct bufferevent *bev, void *arg)
{
	struct connection* conn = (struct connection*)arg;
	conn->bev = bev;
	conn->ev_input = bufferevent_get_input(conn->bev);
	conn->ev_output = bufferevent_get_output(conn->bev);

	for (;;)
	{
		/* Get the number of bytes available in the buffer. */
		conn->len_buf = evbuffer_get_length(conn->ev_input);

		/* Check if there is any data available in the buffer. */
		if (conn->len_buf <= 0)
			return;

		/* Check if the full frame has arrived. */
		if ((conn->bytes_skip == 0) && !can_read_full_frame(conn))
			return;

		/* Skip bytes of oversized frames. */
		if (conn->bytes_skip > 0)
		{
			size_t skip = conn->bytes_skip;

			if (skip > conn->len_buf)
			{
				skip = conn->len_buf;
			}

			log_msg(DEBUG, "Skipping %zu bytes in frame.", skip);
			evbuffer_drain(conn->ev_input, skip);
			conn->bytes_skip -= skip;
			continue;
		}

		/* Process the frame. */
		if (conn->len_frame_payload > 0)
		{
			/* This is a data frame. */
			process_data_frame(conn);
		}
		else
		{
			/* This is a control frame. */
			if (!load_control_frame(conn)) {
				/* Malformed control frame, shut down the connection. */
				cb_close_conn(conn->bev, 0, conn);
				return;
			}

			if (!process_control_frame(conn)) {
				/*
				 * Invalid control state requested, or the
				 * end-of-stream has been reached. Shut down
				 * the connection.
				 */
				cb_close_conn(conn->bev, 0, conn);
				return;
			}
		}
	}
}

// Writes to the socket.
static void cb_write(struct bufferevent *bev, void *arg)
{
	struct connection* conn = (struct connection*)arg;

	if (conn->state != CONN_STATE_STOPPED)
		return;

	cb_close_conn(bev, 0, arg);
}

// Accepts connections from underlying libevent sockets.
static void cb_accept_conn(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *sa, int socklen, void *arg)
{
        struct capture* ctx = (struct capture*)arg;

        // Get the event base from the listener.
        struct event_base *base = evconnlistener_get_base(listener);

        // Set up a buffered event and context for the new connection.
        struct bufferevent* bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
        if (!bev)
        {
                log_msg(ERR, "Failed to accept connection on Unix socket!");
                evutil_closesocket(fd);
                return;
        }

        // Initialize the connection for the accepted socket.
        struct connection* conn = conn_init(ctx);
	if (conn)
	{
	        bufferevent_setcb(bev, cb_read, cb_write, cb_close_conn, (void*)conn);
        	bufferevent_setwatermark(bev, EV_READ, 0, CAPTURE_HIGH_WATERMARK);
	        bufferevent_enable(bev, EV_READ | EV_WRITE);

	        log_msg(INFO, "Accepted new connection on the Unix socket!");
	}
	else
	{
		log_msg(ERR, "Failed to initialize the connection!");
	}

	--ctx->remaining_connections;
	if (ctx->remaining_connections == 0)
	{
		evconnlistener_disable(listener);
	}
}

// Handles errors from underlying libevent sockets.
static void cb_accept_error(struct evconnlistener *listener, void *arg)
{
        const int err = EVUTIL_SOCKET_ERROR();
        log_msg(ERR, "Failed to accept connection on socket: %s", evutil_socket_error_to_string(err));
}

// Initializes the DNStap input for Unbound.
static bool init_dnstap_input()
{
        // If the Honas process was killed ungracefully, the socket file is still present.
        if (access(UNIX_SOCKET_PATH, F_OK) != -1)
        {
                log_msg(INFO, "Unlinking existing socket file %s...", UNIX_SOCKET_PATH);
                unlink(UNIX_SOCKET_PATH);
        }

        // Bind the socket to a file (see define on top of file for default). We already set
        // the first byte of the 'sun_path' to zero with calloc.
        ctx.addr.sun_family = AF_UNIX;
        strncpy(ctx.addr.sun_path, UNIX_SOCKET_PATH, sizeof(ctx.addr.sun_path) - 1);

        // Create the event base.
        ctx.ev_base = event_base_new();
        if (!ctx.ev_base)
        {
                return false;
        }

        // Create the event connection listener.
        unsigned flags = 0;
        flags |= LEV_OPT_CLOSE_ON_FREE; // Closes underlying sockets.
        flags |= LEV_OPT_CLOSE_ON_EXEC; // Sets FD_CLOEXEC on underlying sockets.
        flags |= LEV_OPT_REUSEABLE;      // Sets SO_REUSEADDR on listener.
        ctx.ev_connlistener = evconnlistener_new_bind(ctx.ev_base, cb_accept_conn, (void*)&ctx, flags, -1,
                (struct sockaddr*)&ctx.addr, sizeof(ctx.addr));
        if (!ctx.ev_connlistener)
        {
                event_base_free(ctx.ev_base);
                ctx.ev_base = NULL;
                return false;
        }

        // Set the error handling callback.
        evconnlistener_set_error_cb(ctx.ev_connlistener, cb_accept_error);

	return true;
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

// Parse an item in the configuration.
static int parse_config_item(const char* filename, honas_gather_config_t* config, unsigned int lineno, char* keyword, char* value, unsigned int length)
{
	return honas_gather_config_parse_item(filename, config, lineno, keyword, value, length);
}

// Load the configuration file.
static void load_gather_config(honas_gather_config_t* config, int dirfd, const char* config_file)
{
	log_passert(fchdir(dirfd) != -1, "Failed to change to initial working directory");
	config_read(config_file, config, (parse_item_t*)parse_config_item);
	log_passert(chdir(config->bloomfilter_path) != -1, "Failed to change to honas state directory '%s'", config->bloomfilter_path);
}

// The signal shutdown handler.
static void shutdown_handler(int signum __attribute__((unused)))
{
	event_base_loopexit(ctx.ev_base, NULL);
}

// Sets up signal handlers.
static bool setup_signal_handlers()
{
	// Create a new signal handler for shutdown signals.
	struct sigaction sa = { .sa_handler = shutdown_handler };

	// Set the shutdown signal handlers.
	if (sigemptyset(&sa.sa_mask) != 0)
	{
		return false;
	}
	if (sigaction(SIGTERM, &sa, NULL) != 0)
	{
		return false;
	}
	if (sigaction(SIGINT, &sa, NULL) != 0)
	{
		return false;
	}
	if (sigaction(SIGQUIT, &sa, NULL) != 0)
	{
		return false;
	}

	return true;
}

// --------------------------------------------------------------------------------------------------------

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

static void close_state(honas_state_t* state)
{
	honas_state_persist(state, active_state_file_name, true);
	honas_state_destroy(state);

	log_msg(NOTICE, "Saved honas state to '%s'", active_state_file_name);
}

static void show_usage(const char* program_name, FILE* out)
{
	fprintf(out, "Usage: %s [--help] [--config <file>]\n\n", program_name);
	fprintf(out, "  -h|--help           Show this message\n");
	fprintf(out, "  -c|--config <file>  Load config from file instead of " DEFAULT_HONAS_GATHER_CONFIG_PATH "\n");
	fprintf(out, "  -q|--quiet          Be more quiet (can be used multiple times)\n");
	fprintf(out, "  -s|--syslog         Log messages to syslog\n");
	fprintf(out, "  -v|--verbose        Be more verbose (can be used multiple times)\n");
	fprintf(out, "  -f|--fork           Fork the process as daemon (syslog must be enabled)\n");
	fprintf(out, "  -a|--aggregate      Aggregates queries by subnet per filter (predefined subnets)\n");
	fprintf(out, "  -d|--dry-run        Performs measurements and gives advice about Bloom filter configuration\n");
}

static const struct option long_options[] = {
	{ "help", no_argument, 0, 'h' },
	{ "config", required_argument, 0, 'c' },
	{ "quiet", no_argument, 0, 'q' },
	{ "syslog", no_argument, 0, 's' },
	{ "verbose", no_argument, 0, 'v' },
	{ "fork", no_argument, 0, 'f' },
	{ "aggregate", no_argument, 0, 'a' },
	{ "dry-run", no_argument, 0, 'd' },
	{ 0, 0, 0, 0 }
};

// Finalizes and cleans the Honas instrumentation.
static void finalize_instrumentation()
{
	// Destroy the instrumentation structure.
	instrumentation_destroy(inst_data);

	// Destroy the timer event.
	if (ctx.ev_inst_timer)
	{
		event_free(ctx.ev_inst_timer);
	}

	// Close the instrumentation file.
	if (inst_fd)
	{
		fclose(inst_fd);
	}
}

// The instrumentation timer function, executed every minute.
static void instrumentation_event(evutil_socket_t fd, short what, void *arg)
{
	struct instrumentation* inst_arg = (struct instrumentation*)arg;

	// Dump the instrumentation data to the logfile.
	char dumped[1024];
	instrumentation_dump(inst_arg, dumped, sizeof(dumped));
	fputs(dumped, inst_fd);
	fflush(inst_fd);

	// Reset the instrumentation data.
	instrumentation_reset(inst_arg);

	// Some informative logging.
	log_msg(DEBUG, dumped);
}

// Initializes the Honas instrumentation, writing to a separate logfile.
static void init_instrumentation(struct event_base* base)
{
	// Initialize instrumentation data structure.
	if (!instrumentation_initialize(&inst_data))
	{
		log_msg(ERR, "Failed to initialize instrumentation structure!");
		return;
	}

	// Open the instrumentation logfile.
	inst_fd = fopen(HONAS_INSTRUMENTATION, "ab");
	if (inst_fd)
	{
		log_msg(INFO, "Opened %s for instrumentation information output.", HONAS_INSTRUMENTATION);

		// Set the instrumentation timer.
		struct timeval minutely = { 60, 0 };
		ctx.ev_inst_timer = event_new(base, -1, EV_PERSIST, instrumentation_event, inst_data);
		event_add(ctx.ev_inst_timer, &minutely);
	}
	else
	{
		log_msg(ERR, "Failed to open %s for instrumentation information output!", HONAS_INSTRUMENTATION);
		instrumentation_destroy(inst_data);
	}
}

// Destroys dry run features.
static void dry_run_destroy()
{
	// Destroy HyperLogLog structures.
	hllDestroy(&ctx.dry_run_data.hourly_global);
	hllDestroy(&ctx.dry_run_data.daily_global);

	// Destroy the hourly timer event.
	if (ctx.ev_dry_run_hourly)
	{
		event_free(ctx.ev_dry_run_hourly);
	}

	// Destroy the daily timer event.
	if (ctx.ev_dry_run_daily)
	{
		event_free(ctx.ev_dry_run_daily);
	}

	// Close the dry run output file.
	if (dryrun_fd)
	{
		fclose(dryrun_fd);
	}
}

// Rounds a number up to the nearest multiple.
static unsigned long roundUp(const unsigned long numToRound, const unsigned long multiple)
{
    if (multiple == 0)
        return numToRound;

    unsigned long remainder = numToRound % multiple;
    if (remainder == 0)
        return numToRound;

    return numToRound + multiple - remainder;
}

// Gives advice based on the information collected in a dry run.
static void dry_run_advice()
{
	char date_str[256];

	// Store the current maxima.
	ctx.dry_run_data.hourly_maximum = max(ctx.dry_run_data.hourly_maximum, hllCount(&ctx.dry_run_data.hourly_global, NULL));
	ctx.dry_run_data.daily_maximum = max(ctx.dry_run_data.daily_maximum, hllCount(&ctx.dry_run_data.daily_global, NULL));

	time_t hour = time(NULL);
	strftime(date_str, sizeof(date_str), "%d-%m-%Y %H:%M", localtime(&hour));

	fprintf(dryrun_fd, "------------------------------------ Advice ------------------------------------\n");
	fprintf(dryrun_fd, "[%s] The numbers are rounded up to the nearest hundred-thousand, and a tolerance of 10 percent is added.\n", date_str);
	fprintf(dryrun_fd, "-------------------------------- Hourly Filters --------------------------------\n");

	// Calculate the Bloom filter size and number of hash functions for a false positive rate of 1/1000.
	unsigned long m = roundUp(bloom_filter_size((double)1 / (double)1000, ctx.dry_run_data.hourly_maximum), 100000);
	unsigned long k = optimal_k(ctx.dry_run_data.hourly_maximum, m);
	fprintf(dryrun_fd, "[%s] For a false positive rate of 1 / 1000, BF size (m) should be %lu, based on %llu unique domain names\n"
		, date_str, (unsigned long)(m * 1.1), ctx.dry_run_data.hourly_maximum);
	fprintf(dryrun_fd, "[%s] The number of hash functions (k) should be %lu\n", date_str, k);

	// Calculate the Bloom filter size and number of hash functions for a false positive rate of 1/10000.
	m = roundUp(bloom_filter_size((double)1 / (double)10000, ctx.dry_run_data.hourly_maximum), 100000);
	k = optimal_k(ctx.dry_run_data.hourly_maximum, m);
	fprintf(dryrun_fd, "[%s] For a false positive rate of 1 / 10000, BF size (m) should be %lu, based on %llu unique domain names\n"
		, date_str, (unsigned long)(m * 1.1), ctx.dry_run_data.hourly_maximum);
	fprintf(dryrun_fd, "[%s] The number of hash functions (k) should be %lu\n", date_str, k);

	// Calculate the Bloom filter size and number of hash functions for a false positive rate of 1/100000.
	m = roundUp(bloom_filter_size((double)1 / (double)100000, ctx.dry_run_data.hourly_maximum), 100000);
	k = optimal_k(ctx.dry_run_data.hourly_maximum, m);
	fprintf(dryrun_fd, "[%s] For a false positive rate of 1 / 100000, BF size (m) should be %lu, based on %llu unique domain names\n"
		, date_str, (unsigned long)(m * 1.1), ctx.dry_run_data.hourly_maximum);
	fprintf(dryrun_fd, "[%s] The number of hash functions (k) should be %lu\n", date_str, k);

	fprintf(dryrun_fd, "-------------------------------- Daily Filters ---------------------------------\n");
	time_t day = time(NULL);
	strftime(date_str, sizeof(date_str), "%d-%m-%Y %H:%M", localtime(&day));

	// Calculate the Bloom filter size and number of hash functions for a false positive rate of 1/1000.
	m = roundUp(bloom_filter_size((double)1 / (double)1000, ctx.dry_run_data.daily_maximum), 100000);
	k = optimal_k(ctx.dry_run_data.daily_maximum, m);
	fprintf(dryrun_fd, "[%s] For a false positive rate of 1 / 1000, BF size (m) should be %lu, based on %llu unique domain names\n"
		, date_str, (unsigned long)(m * 1.1), ctx.dry_run_data.daily_maximum);
	fprintf(dryrun_fd, "[%s] The number of hash functions (k) should be %lu\n", date_str, k);

	// Calculate the Bloom filter size and number of hash functions for a false positive rate of 1/10000.
	m = roundup(bloom_filter_size((double)1 / (double)10000, ctx.dry_run_data.daily_maximum), 100000);
	k = optimal_k(ctx.dry_run_data.daily_maximum, m);
	fprintf(dryrun_fd, "[%s] For a false positive rate of 1 / 10000, BF size (m) should be %lu, based on %llu unique domain names\n"
		, date_str, (unsigned long)(m * 1.1), ctx.dry_run_data.daily_maximum);
	fprintf(dryrun_fd, "[%s] The number of hash functions (k) should be %lu\n", date_str, k);

	// Calculate the Bloom filter size and number of hash functions for a false positive rate of 1/100000.
	m = roundUp(bloom_filter_size((double)1 / (double)100000, ctx.dry_run_data.daily_maximum), 100000);
	k = optimal_k(ctx.dry_run_data.daily_maximum, m);
	fprintf(dryrun_fd, "[%s] For a false positive rate of 1 / 100000, BF size (m) should be %lu, based on %llu unique domain names\n"
		, date_str, (unsigned long)(m * 1.1), ctx.dry_run_data.daily_maximum);
	fprintf(dryrun_fd, "[%s] The number of hash functions (k) should be %lu\n", date_str, k);

	fprintf(dryrun_fd, "-------------------------------------- End -------------------------------------\n");
}

// The dry run instrumentation event function for hourly dumps.
static void dry_run_hourly_event(evutil_socket_t fd, short what, void *arg)
{
	struct dry_run_counters* data = (struct dry_run_counters*)arg;
	char date_str[256];

	time_t hour = time(NULL);
	strftime(date_str, sizeof(date_str), "%d-%m-%Y %H:%M", localtime(&hour));

	// Write the hourly dry run statistics to the output file.
	const unsigned long long current = hllCount(&data->hourly_global, NULL);
	fprintf(dryrun_fd, "[%s] Distinct count this hour: %llu, total query count: %llu\n", date_str, current, data->hourly_total_queries);
	fflush(dryrun_fd);

	// Set maximum if necessary.
	data->hourly_maximum = max(data->hourly_maximum, current);

	// Reinitialize the HyperLogLog counter.
	hllDestroy(&data->hourly_global);
	hllInit(&data->hourly_global);
	data->hourly_total_queries = 0;

	// Check if the daily midnight dump should be performed.
	struct tm current_time;
	if (gmtime_r(&hour, &current_time)->tm_hour == 0)
	{
		// Dump the current advice.
		dry_run_advice();
	}
}

// The dry run instrumentation event function for daily dumps.
static void dry_run_daily_event(evutil_socket_t fd, short what, void *arg)
{
	struct dry_run_counters* data = (struct dry_run_counters*)arg;
	char date_str[256];

	time_t day = time(NULL);
	strftime(date_str, sizeof(date_str), "%d-%m-%Y", localtime(&day));

	// Write the hourly dry run statistics to the output file.
	const unsigned long long current = hllCount(&data->daily_global, NULL);
	fprintf(dryrun_fd, "[%s] Distinct count this day: %llu, total query count: %llu\n", date_str, current, data->daily_total_queries);
	fflush(dryrun_fd);

	// Set maximum if necessary.
	data->daily_maximum = max(data->daily_maximum, current);

	// Reinitialize the HyperLogLog counter.
	hllDestroy(&data->daily_global);
	hllInit(&data->daily_global);
	data->daily_total_queries = 0;
}

// Adds periodic events for the dumping of dry-run instrumentation data.
static void init_dry_run_dumps(struct event_base* base)
{
	// Open the dry run output file.
	dryrun_fd = fopen(HONAS_DRYRUNFILE, "ab");
	if (!dryrun_fd)
	{
		log_msg(ERR, "Failed to initialize dry-run output file! No information will be logged.");
		return;
	}

	// Add dry run instrumentation event for hourly dumps.
	struct timeval hourly = { 3600, 0 };
	ctx.ev_dry_run_hourly = event_new(base, -1, EV_PERSIST, dry_run_hourly_event, &ctx.dry_run_data);
	event_add(ctx.ev_dry_run_hourly, &hourly);

	// Add dry run instrumentation event for daily dumps.
	struct timeval daily = { 86400, 0 };
	ctx.ev_dry_run_daily = event_new(base, -1, EV_PERSIST, dry_run_daily_event, &ctx.dry_run_data);
	event_add(ctx.ev_dry_run_daily, &daily);

	// Initialize HyperLogLog counters.
	hllInit(&ctx.dry_run_data.hourly_global);
	hllInit(&ctx.dry_run_data.daily_global);
}

// Helper function that takes the time difference in milliseconds from two timeval structs.
// Taken from StackOverflow.
static const float timedifference_msec(struct timeval* t0, struct timeval* t1)
{
    return (t1->tv_sec - t0->tv_sec) * 1000.0f + (t1->tv_usec - t0->tv_usec) / 1000.0f;
}

// The signal reload/recheck handler.
static void recheck_handler(evutil_socket_t fd, short what, void *arg)
{
	honas_state_t* state_param = (honas_state_t*)arg;

	const uint64_t now = time(NULL);
	const int64_t wait = state_param->header->period_end - now;
	if (wait <= 0)
	{
		// Finalize current state, reload config and create new current state
		finalize_state(state_param);
		load_gather_config(&config, init_dirfd, config_file);
		create_state(&config, state_param, now);

		// Reset the false positive rate threshold warning.
		ctx.fpr_warning_passed = false;

		// Also reinitialize the subnet activity configuration, as this configuration
		// may change. Automatic reloading is the most convinient.
		if (ctx.aggregate_subnets)
		{
			// Measure the time it takes to reload the subnet activity configuration.
			struct timeval t_stop, t_start;
			gettimeofday(&t_start, NULL);

			// Destroy the subnet activity subsystem.
			if (subnet_activity_destroy(&ctx.subnet_metadata) == SA_OK)
			{
				log_msg(INFO, "Finalized subnet activity resources.");
			}
			else
			{
				log_msg(ERR, "Failed to finalize subnet activity resources in recheck handler!");
			}

			// Reinitialize the subnet aggregation subsystem.
			if (subnet_activity_initialize(config.subnet_activity_path, &ctx.subnet_metadata) == SA_OK)
			{
				log_msg(INFO, "Succesfully initialized the subnet aggregation subsystem!");
			}
			else
			{
				log_msg(ERR, "Failed to initialize the subnet aggregation subsystem!");
			}

			// Log the time it took.
			gettimeofday(&t_stop, NULL);
			log_msg(INFO, "Subnet activity configuration reload took %f ms", timedifference_msec(&t_start, &t_stop));
		}
	}
}

// The Honas gather entrypoint.
int main(int argc, char** argv)
{
	const char* program_name = "honas-gather";
	bool daemonize = false;
	bool syslogenabled = false;

	// Set entire capture context initially to zero.
	memset(&ctx, 0, sizeof(struct capture));

	/* Parse command line arguments */
	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "hc:qsvfad", long_options, &option_index);
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
			syslogenabled = true;
			break;

		case 'v':
			log_set_min_log_level(log_get_min_log_level() + 1);
			break;

		case '?':
			show_usage(program_name, stderr);
			return 1;

		case 'f':
			daemonize = true;
			break;

		case 'a':
			ctx.aggregate_subnets = true;
			break;

		case 'd':
			ctx.dry_run = true;
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
			log_msg(DEBUG, "PID of forked process is %d", process_id);
			return 0;
		}
	}

	log_msg(INFO, "%s (version %s)", program_name, VERSION);

	/* Setup signal handlers */
	if (!setup_signal_handlers())
	{
		log_msg(ERR, "Failed to install signal handlers!");
		return 1;
	}

	/* Open current working directory for consistent relative config file loading */
	init_dirfd = open(".", O_PATH | O_DIRECTORY | O_CLOEXEC);
	log_passert(init_dirfd != -1, "Failed to open initial working directory");

	/* Initialize and read configuration */
	honas_gather_config_init(&config);
	load_gather_config(&config, init_dirfd, config_file);
	honas_gather_config_finalize(&config);

	/* Open or create honas state for this period */
	if (!try_open_active_state(&current_active_state)) {
		create_state(&config, &current_active_state, time(NULL));
	}

	// Start up the state rotation process. The recheck handler will schedule alarms.
	recheck_handler(0, 0, &current_active_state);

	// Log a warning about the filter size if applicable.
	const uint32_t req_entropy = honas_state_calculate_required_entropy(&current_active_state);
	if (req_entropy > 512) // Maximum we can use is SHA-512!
	{
		log_msg(WARN, "The required entropy for the current filter size is %i, which is larger than SHA-512 can offer!");
	}

	// Initialize the DNStap input feature.
	if (!init_dnstap_input())
	{
		log_msg(ERR, "Failed to initialize DNStap input!");
		return 1;
	}
	else
	{
		log_msg(INFO, "Initialized DNStap input!");
	}

	// Add a recurring event each minute to perform state rotation.
	struct timeval each_minute = { 60, 0 };
	ctx.ev_state_rotation = event_new(ctx.ev_base, -1, EV_PERSIST, recheck_handler, &current_active_state);
	event_add(ctx.ev_state_rotation, &each_minute);

	// Initialize Honas instrumentation.
	init_instrumentation(ctx.ev_base);

	// Initialize subnet aggregation if requested.
	if (ctx.aggregate_subnets)
	{
		// Initialize the subnet aggregation subsystem.
		if (subnet_activity_initialize(config.subnet_activity_path, &ctx.subnet_metadata) == SA_OK)
		{
			log_msg(INFO, "Succesfully initialized the subnet aggregation subsystem!");
		}
		else
		{
			log_msg(ERR, "Failed to initialize the subnet aggregation subsystem!");
		}
	}

	// Performing dry-run to measure and advise operator about parameters.
	if (ctx.dry_run)
	{
		// Initialize dry run features.
		init_dry_run_dumps(ctx.ev_base);
		log_msg(INFO, "Performing dry-run: measuring data and giving advice about Bloom filter configuration.");
	}

	// Allow infinitely many connections.
	ctx.remaining_connections = -1;

	// Run the main processing loop (listen for events on DNStap).
	log_msg(INFO, "Starting main processing loop...");
	if (event_base_dispatch(ctx.ev_base) != 0)
	{
		log_msg(ERR, "The main processing loop failed to start!");
		return 1;
	}

	// Finalize application.
	log_msg(NOTICE, "Done processing");

	// Unlink socket file.
	log_msg(INFO, "Unlinking socket file %s...", UNIX_SOCKET_PATH);
	unlink(UNIX_SOCKET_PATH);

	// Free the state rotation event.
	if (ctx.ev_state_rotation)
	{
		event_free(ctx.ev_state_rotation);
	}

	// Clean up and finalize instrumentation.
	finalize_instrumentation();

	// If a dry run was performed, provide advice about the parameters for Bloom filters.
	if (ctx.dry_run)
	{
		// Calculate parameters k and m, based on the maximum hourly and daily distinct domain names.
		// Give advice on the parameters for three possible false positive rates.
		dry_run_advice();

		// Clean up and finalize dry run features.
		dry_run_destroy();
	}

	// Clean up the subnet activity resources if necessary.
	if (ctx.aggregate_subnets && subnet_activity_destroy(&ctx.subnet_metadata) == SA_OK)
	{
		log_msg(INFO, "Finalized subnet activity resources.");
	}

	// Clean up libevent resources.
	if (ctx.ev_connlistener != NULL)
	{
		evconnlistener_free(ctx.ev_connlistener);
	}
	if (ctx.ev_base != NULL)
	{
		event_base_free(ctx.ev_base);
	}

	/* Clean shutdown; persist active current state */
	close_state(&current_active_state);

	/* Destroy previously initialized data */
	honas_gather_config_destroy(&config);

	log_msg(NOTICE, "Exiting");
	log_destroy();
	return 0;
}
