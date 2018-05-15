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

// Dumps the instrumentation data so far. str_length is the maximum length of out_str.
void instrumentation_dump(struct instrumentation* p_inst, char* const out_str, const size_t str_length)
{
	if (p_inst && out_str)
	{
		// Calculate aggregated fields.
		p_inst->n_queries_sec = p_inst->n_processed_queries / INSTRUMENTATION_INTERVAL_SEC;

		// Dump the instrumentation data to a structured single-line string.
		snprintf(out_str, str_length, "Instrumentation: n_proc=%zu,n_acc=%zu,n_skip=%zu,n_qsec=%zu\n"
			, p_inst->n_processed_queries, p_inst->n_accepted_queries, p_inst->n_skipped_queries
			, p_inst->n_queries_sec);
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
	}
}
