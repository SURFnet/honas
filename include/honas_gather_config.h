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

#ifndef HONAS_GATHER_CONFIG_H
#define HONAS_GATHER_CONFIG_H

#include "honas_state.h"

/// \defgroup honas_gather_config Honas gather configuration

/** Honas gather configuration
 */
typedef struct {
	char* bloomfilter_path;
	char* input_name;
	uint32_t period_length;
	uint32_t number_of_filters;
	uint32_t number_of_bits_per_filter;
	uint32_t number_of_hashes;
	uint32_t number_of_filters_per_user;
	uint32_t flatten_threshold;
} honas_gather_config_t;

/** Initialize honas gather configuration structure
 *
 * \param config The honas gather configuration structure to be initialized
 * \ingroup honas_gather_config
 */
extern void honas_gather_config_init(honas_gather_config_t* config);

/** Cleanup the honas gather configuration structure
 *
 * This method should be called when the honas gather config is no longer needed
 * to release any resources associated with it. Otherwise resource leakage might
 * occur.
 *
 * \param config The honas gather configuration structure that is to be destroyed
 * \ingroup honas_gather_config
 */
extern void honas_gather_config_destroy(honas_gather_config_t* config);

/** Callback function that can be used to parse config items using `config_read()`
 *
 * \param filename  The filename of the config file containing the config item
 * \param config    The honas gather config structure that is to be filled
 * \param lineno    The line number in the config file the config item was encountered
 * \param keyword   The nul-terminated config item keyword without any leading or trailing whitespace
 * \param value     The nul-terminated config value without any leading or trailing whitespace
 * \param length    The length of the config value (aka: the index of the nul-termination)
 * \returns 1 if the config item was recognized, 0 otherwise
 * \ingroup honas_gather_config
 */
extern int honas_gather_config_parse_item(const char* filename, honas_gather_config_t* config, unsigned int lineno, char* keyword, char* value, unsigned int length);

/** Finalize the honas gather config that was read
 *
 * This method will check if all the config options have sensible values.
 *
 * \param config The honas gather configuration that should be properly filled
 * \ingroup honas_gather_config
 */
extern void honas_gather_config_finalize(honas_gather_config_t* config);

#endif /* HONAS_GATHER_CONFIG_H */
