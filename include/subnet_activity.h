/*
 * Copyright (c) 2018 Gijs Rijnders, SURFnet
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

#ifndef HONAS_SUBNET_ACTIVITY_H
#define HONAS_SUBNET_ACTIVITY_H

#include "inet.h"
#include <stdbool.h>

// Defines a prefix.
struct prefix_match
{
	// The IP address.
	struct in_addr46 prefix;

	// The prefix length.
	unsigned int length;
};

// Defines an entity that has prefixes assigned.
struct entity
{
	// The name of the entity.
	char name[128];

	// A list of prefixes that are associated to this entity.
	struct prefix_match* associated_prefixes[32];

	// The number of prefixes currently associated.
	size_t ass_prefix_count;
};

// The subnet activity metadata structure.
struct subnet_activity
{
	// A list of entities that are registered for testing subnet activity.
	struct entity* entities[1024];

	// The number of entities currently registered.
	size_t registered_entities;

	// A list of prefixes that are registered.
	struct prefix_match* prefixes[1024];

	// The number of prefixes currently registered.
	size_t registered_prefixes;
};

// Loads the subnet activity file and initializes the required logic.
const bool subnet_activity_initialize(const char* subnetfile, struct subnet_activity* p_subact);

// Tests an IP-address to a specific entity. The entity and prefix is returned.
const bool subnet_activity_match_prefix(const struct in_addr46* client, const sa_family_t af, struct subnet_activity* p_subact);

// Destroys and frees the subnet activity structure.
void subnet_activity_destroy(struct subnet_activity* p_subact);

#endif /* HONAS_SUBNET_ACTIVITY_H */
