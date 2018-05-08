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

#ifndef HONAS_INPUT_H
#define HONAS_INPUT_H

#include "config.h"
#include "honas_state.h"

/** Callback function used to initialize some possible input state
 *
 * This function will always be called for all input modules, even those that
 * will not be used.
 *
 * The 'input_state_ptr' variable can be used to create a reference that is
 * accessible for calls to the other functions.
 *
 * \param input_state_ptr A pointer that can be updated to point at the honas input state context
 * \returns '0' on success or '-1' on system errors (errno is set appropriately)
 */
typedef int(input_init_fn_t)(void** input_state_ptr);

/** Callback function that gets called to cleanup the input state
 *
 * This function will always be called for all input modules, even those that
 * will not be used.
 *
 * There is no guarantee that this function will be called, it will probably
 * only be called during a clean shutdown of the program.
 *
 * \param input_state Pointer to the honas input state context that is to be destroyed
 */
typedef void(input_destroy_fn_t)(void* input_state);

/** Callback function used to finalize the input configuration
 *
 * This function is only called for the selected input method and can be used to
 * perform configuration checks and for opening possible input channels.
 *
 * The 'parse_config_item' function will also get called after this function has
 * been called during config reloads. This function however will not get called
 * again.
 *
 * \param input_state The honas input state context
 */
typedef void(input_finalize_config_fn_t)(void* input_state);

/** Callback function that gets called to read the next input
 *
 * This function is only called for the selected input method and will be called
 * multiple times.
 *
 * The calling function will only read from the host name pointed to by
 * 'host_name' up to the returned length in bytes and will never make changes
 * to the indicated value.
 *
 * The values of 'client' and 'host_name' are only inspected if a size larger than
 * '0' is returned and are otherwise ignored.
 *
 * If this function receives an EINTR or EAGAIN error when making a system call
 * then it must return to the caller with a '-1' return status and the original
 * error code in 'errno'.
 *
 * \param input_state The honas input state context
 * \param client      Pointer to the client structure that is to be updated with the client's details
 * \param host_name   Pointer that needs to be updated to point at the beginning of the host_name
 * \returns The length of the `host_name` on success,
 *          0 if the end of the input has been reached
 *          or -1 on error (errno must be set appropriately)
 */
typedef ssize_t(input_next_fn_t)(void* input_state, struct in_addr46 *client, uint8_t **host_name);

/** Structure describing a honas input and is to be filled by specific honas input modules */
typedef struct {
	/** The name of the honas input module */
	const char* name;
	/** Optional callback function for initializing the input module state */
	input_init_fn_t* init;
	/** Optional callback function for processing possible input module related config options */
	parse_item_t* parse_config_item;
	/** Optional callback function for activating the input module based on the read config */
	input_finalize_config_fn_t* finalize_config;
	/** Required callback function for retrieving the next host name lookup */
	input_next_fn_t* next;
	/** Optional callback function for cleaning up the input module state */
	input_destroy_fn_t* destroy;
} honas_input_t;

#endif /* HONAS_INPUT_H */
