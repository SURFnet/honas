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

#include "includes.h"
#include "honas_input.h"
#include "input_dns_relayd.h"
#include "logging.h"

static const honas_input_t* input_modules[] = { &input_dns_relayd };
static const size_t input_modules_count = sizeof(input_modules) / sizeof(input_modules[0]);

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <input-module-name>\n", argv[0]);
		return 1;
	}

	log_set_min_log_level(EMERG);

	const honas_input_t *input_module = NULL;
	for (int i = 0; i < input_modules_count; i++) {
		if (strcmp(input_modules[i]->name, argv[1]) == 0) {
			input_module = input_modules[i];
		}
	}
	if (input_module == NULL) {
		fprintf(stderr, "Unknown input module!\n");
		abort(); /* We abort so that afl-fuzz will warn when this happens! */
	}

	void *state = NULL;
	input_module->init(&state);
	if (input_module->finalize_config != NULL)
		input_module->finalize_config(state);

	struct in_addr46 client = { 0 };
	uint8_t *host_name = NULL;
	while (1) {
		ssize_t result = input_module->next(state, &client, &host_name);
		if (result == 0)
			break;
		assert(result > 0);
	}

	input_module->destroy(state);
}
