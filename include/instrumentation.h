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

#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

// Default instrumentation interval.
#define INSTRUMENTATION_INTERVAL_SEC	60

#include <stddef.h>
#include <stdbool.h>
#include <ldns/ldns.h>

// The subnet aggregation structure for instrumentation contains information about
// subnet activity in Honas.
struct subnet_instrumentation
{
	// Specifies the number of queries that were categorized in a subnet.
	size_t				n_queries_in_subnet;

	// Specifies the number of queries that were not categorized in a subnet.
	size_t				n_queries_not_in_subnet;
};

// The instrumentation data structure that will be written to a file periodically.
struct instrumentation
{
	// Specifies the total number of processed queries.
	size_t				n_processed_queries;

	// Specifies the number of skipped (invalid and irrelevant) queries.
	size_t				n_skipped_queries;

	// Specifies the number of accepted (relevant) queries.
	size_t				n_accepted_queries;

	// Aggregate field that specifies the number of processed queries per second.
	size_t				n_queries_sec;

	// Specifies the number of A queries.
	size_t				n_a_queries;

	// Specifies the number of AAAA queries.
	size_t				n_aaaa_queries;

	// Specifies the number of NS queries.
	size_t				n_ns_queries;

	// Specifies the number of MX queries.
	size_t				n_mx_queries;

	// Specifies the number of PTR queries.
	size_t				n_ptr_queries;

	// Specifies the current memory usage of the process in kilobytes.
	size_t				memory_usage_kb;

	// Contains information about DNS queries in subnets.
	struct subnet_instrumentation	subnet_aggregates;

	// Specifies the number of invalid frames received by the listener.
	size_t				n_invalid_frames;
};

// Increments and updates the number of processed queries.
void instrumentation_increment_processed(struct instrumentation* p_inst);

// Increments and updates the number of accepted queries.
void instrumentation_increment_accepted(struct instrumentation* p_inst);

// Increments and updates the number of skipped queries.
void instrumentation_increment_skipped(struct instrumentation* p_inst);

// Increments and updates the number of queries per type (A, AAAA, NS, MX).
void instrumentation_increment_type(struct instrumentation* p_inst, const ldns_rr_type qtype);

// Dumps the instrumentation data so far. str_length is the maximum length of out_str.
void instrumentation_dump(struct instrumentation* p_inst, char* const out_str, const size_t str_length);

// Resets the instrumentation data so far.
void instrumentation_reset(struct instrumentation* p_inst);

// Allocates and initializes a new instrumentation structure.
const bool instrumentation_initialize(struct instrumentation** pp_inst);

// Destroys an instrumentation structure.
void instrumentation_destroy(struct instrumentation* p_inst);

// Updates the subnet activity data structure.
void instrumentation_update_subnet_activity(struct instrumentation* p_inst, const size_t in, const size_t not_in);

// Increments the number of invalid frames received by the listener.
void instrumentation_increment_invalid(struct instrumentation* p_inst);

#endif // INSTRUMENTATION_H
