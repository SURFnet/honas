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

/*
 * This file contains instrumentation logic for Honas.
 */

#include "instrumentation.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>

// Increments and updates the number of processed queries.
void instrumentation_increment_processed(struct instrumentation* p_inst)
{
	if (p_inst)
	{
		++p_inst->n_processed_queries;
	}
}

// Increments and updates the number of accepted queries.
void instrumentation_increment_accepted(struct instrumentation* p_inst)
{
	if (p_inst)
	{
		++p_inst->n_accepted_queries;
	}
}

// Increments and updates the number of skipped queries.
void instrumentation_increment_skipped(struct instrumentation* p_inst)
{
	if (p_inst)
	{
		++p_inst->n_skipped_queries;
	}
}

// Increments and updates the number of queries per type (A, AAAA, NS, MX).
void instrumentation_increment_type(struct instrumentation* p_inst, const ldns_rr_type qtype)
{
	if (p_inst)
	{
		switch (qtype)
		{
			case LDNS_RR_TYPE_A:
			++p_inst->n_a_queries;
				break;
			case LDNS_RR_TYPE_AAAA:
			++p_inst->n_aaaa_queries;
				break;
			case LDNS_RR_TYPE_NS:
			++p_inst->n_ns_queries;
				break;
			case LDNS_RR_TYPE_MX:
			++p_inst->n_mx_queries;
				break;
			case LDNS_RR_TYPE_PTR:
			++p_inst->n_ptr_queries;
			default:
				break;
		}
	}
}

// Dumps the instrumentation data so far. str_length is the maximum length of out_str.
void instrumentation_dump(struct instrumentation* p_inst, char* const out_str, const size_t str_length)
{
	if (p_inst && out_str)
	{
		// Calculate aggregated fields.
		p_inst->n_queries_sec = p_inst->n_processed_queries / INSTRUMENTATION_INTERVAL_SEC;

		// Get usage information about the running process.
		struct rusage r_usage;
		getrusage(RUSAGE_SELF,&r_usage);

		// Set the resource information accordingly.
		p_inst->memory_usage_kb = r_usage.ru_maxrss;

		// Dump the instrumentation data to a structured single-line string.
		snprintf(out_str, str_length, "Instrumentation: n_proc=%zu,n_acc=%zu,n_skip=%zu,n_qsec=%zu,n_qa=%zu,n_qaaaa=%zu,n_qns=%zu,n_qmx=%zu,n_qptr=%zu,mem_usg_kb=%zu,n_qcat=%zu,n_qncat=%zu,n_invfrm=%zu\n"
			, p_inst->n_processed_queries, p_inst->n_accepted_queries, p_inst->n_skipped_queries
			, p_inst->n_queries_sec, p_inst->n_a_queries, p_inst->n_aaaa_queries
			, p_inst->n_ns_queries, p_inst->n_mx_queries, p_inst->n_ptr_queries, p_inst->memory_usage_kb
			, p_inst->subnet_aggregates.n_queries_in_subnet, p_inst->subnet_aggregates.n_queries_not_in_subnet
			, p_inst->n_invalid_frames);
	}
}

// Resets the instrumentation data so far.
void instrumentation_reset(struct instrumentation* p_inst)
{
	if (p_inst)
	{
		p_inst->n_processed_queries = 0;
		p_inst->n_accepted_queries = 0;
		p_inst->n_skipped_queries = 0;
		p_inst->n_queries_sec = 0;
		p_inst->n_a_queries = 0;
		p_inst->n_aaaa_queries = 0;
		p_inst->n_ns_queries = 0;
		p_inst->n_mx_queries = 0;
		p_inst->n_ptr_queries = 0;
		p_inst->subnet_aggregates.n_queries_in_subnet = 0;
		p_inst->subnet_aggregates.n_queries_not_in_subnet = 0;
		p_inst->n_invalid_frames = 0;
	}
}

// Allocates and initializes a new instrumentation structure.
const bool instrumentation_initialize(struct instrumentation** pp_inst)
{
	if (pp_inst)
	{
		// Allocate and zero-initialize a new instrumentation structure.
		*pp_inst = (struct instrumentation*)calloc(1, sizeof(struct instrumentation));
		if (*pp_inst)
		{
			return true;
		}
	}

	return false;
}

// Destroys an instrumentation structure.
void instrumentation_destroy(struct instrumentation* p_inst)
{
	if (p_inst)
	{
		free(p_inst);
	}
}

// Updates the subnet activity data structure.
void instrumentation_update_subnet_activity(struct instrumentation* p_inst, const size_t in, const size_t not_in)
{
	if (p_inst)
	{
		p_inst->subnet_aggregates.n_queries_in_subnet += in;
		p_inst->subnet_aggregates.n_queries_not_in_subnet += not_in;
	}
}

// Increments the number of invalid frames received by the listener.
void instrumentation_increment_invalid(struct instrumentation* p_inst)
{
	if (p_inst)
	{
		++p_inst->n_invalid_frames;
	}
}
