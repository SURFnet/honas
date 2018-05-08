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

/**
 * This file contains a simple configuration file parser.
 *
 * \defgroup config Config file handling
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "includes.h"

/** Type of a callback function that gets called for each config item
 *
 * The callback function must return `true` for any config item that it has recognized,
 * failure to do so would lead to the config reader exiting with an unrecognized config
 * option error.
 *
 * \param filename  The filename of the config file containing the config item
 * \param ptr       An opaque pointer that can be used to pass state to the callback function
 * \param lineno    The line number in the config file the config item was encountered
 * \param keyword   The nul-terminated config item keyword without any leading or trailing whitespace
 * \param value     The nul-terminated config value without any leading or trailing whitespace
 * \param length    The length of the config value (aka: the index of the nul-termination)
 * \returns 1 if the config item was processed, 0 otherwise
 */
typedef int(parse_item_t)(const char* filename, void* ptr, unsigned int lineno, char* keyword, char* value, unsigned int length);

/** Read config from file
 *
 * \param filename   The filename of the config file to read
 * \param ptr        An opaque pointer that get's passed verbatim to the `parse_item` callback function
 * \param parse_item Pointer to a function that gets called for each config item encountered in the config
 * \ingroup config
 */
extern void config_read(const char* filename, void* ptr, parse_item_t* parse_item);

#endif /* CONFIG_H */
