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

#include "honas_gather_config.h"

#include "logging.h"
#include "utils.h"

void honas_gather_config_init(honas_gather_config_t* config)
{
	config->bloomfilter_path = NULL;
	config->period_length = 0;
	config->number_of_filters = 0;
	config->number_of_bits_per_filter = 0;
	config->number_of_hashes = 0;
	config->number_of_filters_per_user = 0;
	config->flatten_threshold = 0;
}

static char* string_value(char* keyword, char* value)
{
	char* result = strdup(value);
	log_passert(result != NULL, "Failed to allocate string for config option '%s'", keyword);
	return result;
}

static uint32_t uint32_value(char* keyword, char* value)
{
	uint32_t result;
	if (!my_strtouint32(value, &result, NULL, 10))
		log_die("Invalid value for '%s'", keyword);
	return result;
}

#define _config_parse_and_check_value(field, parse_function, check)                                  \
	do {                                                                                             \
		if (strcmp(keyword, #field) == 0) {                                                          \
			config->field = parse_function(keyword, value);                                          \
			__typeof__(config->field) value = (config->field);                                       \
			if (!(check))                                                                            \
				log_die("Invalid value for config option '" #field "', failed check: `" #check "`"); \
			parsed = 1;                                                                              \
		}                                                                                            \
	} while (0)

int honas_gather_config_parse_item(const char* UNUSED(filename), honas_gather_config_t* config, unsigned int UNUSED(lineno), char* keyword, char* value, unsigned int UNUSED(length))
{
	int parsed = 0;

	if (strcmp(keyword, "bloomfilter_path") == 0 && config->bloomfilter_path != NULL)
		free(config->bloomfilter_path);

	_config_parse_and_check_value(bloomfilter_path, string_value, strlen(value) > 0);
	_config_parse_and_check_value(period_length, uint32_value, value > 0);
	_config_parse_and_check_value(number_of_filters, uint32_value, value > 0);
	_config_parse_and_check_value(number_of_bits_per_filter, uint32_value, value > 0);
	_config_parse_and_check_value(number_of_hashes, uint32_value, value > 0);
	_config_parse_and_check_value(number_of_filters_per_user, uint32_value, value > 0);
	_config_parse_and_check_value(flatten_threshold, uint32_value, value > 0);
	return parsed;
}

#define _config_check_set(field, unset_value)                           \
	do {                                                                \
		__typeof__(config->field) _unset = (unset_value);               \
		if ((config->field) == _unset) {                                \
			log_msg(WARN, "Unset required config option '" #field "'"); \
			valid = false;                                              \
		}                                                               \
	} while (0)

void honas_gather_config_finalize(honas_gather_config_t* config)
{
	bool valid = true;
	_config_check_set(bloomfilter_path, NULL);
	_config_check_set(period_length, 0);
	_config_check_set(number_of_filters, 0);
	_config_check_set(number_of_bits_per_filter, 0);
	_config_check_set(number_of_hashes, 0);
	_config_check_set(number_of_filters_per_user, 0);
	if (!valid)
		log_die("There were config errors");
}

void honas_gather_config_destroy(honas_gather_config_t* config)
{
	if (config->bloomfilter_path != NULL) {
		free(config->bloomfilter_path);
		config->bloomfilter_path = NULL;
	}
}
