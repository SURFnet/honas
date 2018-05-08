/*
 * This file contains the main honas state file handling API.
 */

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

#ifndef HONAS_STATE_H
#define HONAS_STATE_H

#include "bitset.h"
#include "hyperloglog.h"
#include "includes.h"
#include "inet.h"

#define HONAS_STATE_FILE_MAGIC "DNSBLOOM"
#define CURRENT_HONAS_STATE_MAJOR_VERSION 1
#define CURRENT_HONAS_STATE_MINOR_VERSION 0

/** Honas state
 *  ===========
 *
 * The Honas state represents the file with all the data collected within a
 * certain period.
 *
 * The Honas state can be updated to include additional host name lookups and
 * can be queries to see which host names have been looked up. It contains all
 * the information needed to interpret the data (one doesn't need to know the
 * configuration that was used when creating it).
 *
 * Creating, loading and saving states
 * -----------------------------------
 *
 * A honas state can be freshly created using `honas_state_create()` or read
 * from a file using `honas_state_load()`.
 *
 * After reading the honas state as read-write from a file, all changes are
 * being made on an in memory snapshot of that state. To make the changes
 * available to other programs the `honas_state_persist()` function must be
 * used to write the new state data to a file.
 *
 * \note The creation and rotation of state files for different periods and the
 *       setting of the `period_begin` and `period_end` fields is a concern of
 *       the code calling these functions.
 *
 * Registering host name lookups
 * -----------------------------
 *
 * To register a host name lookup one must call the
 * `honas_state_register_host_name_lookup()` function. This function will:
 *
 * - Update the first/last request timestamp values.
 * - Update the clients cardinality estimation data for the client that made
 *   the request.
 * - Register the hash of the host name and all parent domains (except for the
 *   TLD) in a subset of all the bloomfilters in the honas state. The selection
 *   of which bloomfilter(s) to update is made based on the client.
 * - Update the host name cardinality estimation fort each of the domains being
 *   updated in the bloomfilters.
 *
 * \note This function does not perform any checks whatsoever to make sure that
 *       the host name request timestamp is within the period described by this
 *       state.
 *
 * Check for host name lookups
 * ---------------------------
 *
 *  To check if a host name has been looked up one must call the
 *  `honas_state_check_host_name_lookups()` function with a hash of the host
 *  name.
 *
 *  This function will test the separate bloomfilters and report back the
 *  number of bloomfilters which probably contain that hash. Optionally it can
 *  also report which of the bloomfilters probably contained that hash.
 *
 *  This count can be checked against the number of bloomfilters that should be
 *  updated per host name lookup by a user to get a very coarse estimate of
 *  the number of users that probably requested the host name.
 *
 *  The list of bloomfilters with hits can be used as a basis to check if
 *  multiple host names were requested by the same set of users.
 *
 * \defgroup honas_state Honas state operations
 */

/** Honas state file header
 *
 * Honas state file follow SemVer versioning semantics.  This means additions
 * can be made as long as they are backwards compatible by increasing the minor
 * number.
 *
 * \note All integers are in little endian byte order
 */
struct honas_state_file_header {
	char file_magic[8];     ///< Honas state file identification string (`DNSBLOOM`)
	uint32_t major_version; ///< State file major version
	uint32_t minor_version; ///< State file minor version

	// Bloomfilter configuration
	uint32_t first_filter_offset;        ///< Start of the first filter from the beginning of the state file
	uint32_t padding_after_filters;      ///< Number of bytes after each filter
	uint32_t number_of_filters;          ///< Number of filters inside the state file
	uint32_t number_of_bits_per_filter;  ///< Number of bits each of the filters
	uint32_t number_of_hashes;           ///< The number of hashes that should be set in each filter for every value
	uint32_t number_of_filters_per_user; ///< The number of filters that should be updated for each users
	uint32_t flatten_threshold;          ///< The threshold of estimated distinct clients below which the search results should be flattened for the given properties

	// Hyperloglog configuration
	uint32_t client_hll_size;             ///< Size of the client hyperloglog data
	uint32_t padding_after_client_hll;    ///< Number of bytes after the client hyperloglog data
	uint32_t host_name_hll_size;          ///< Size of the host name hyperloglog data
	uint32_t padding_after_host_name_hll; ///< Number of bytes after the host name hyperloglog data

	// Period information
	uint64_t period_begin;       ///< Timestamp of the beginning of the period ("create time")
	uint64_t period_end;         ///< Timestamp of the end of the period (planned "persist time")
	uint64_t first_request;      ///< Timestamp of the first request processed
	uint64_t last_request;       ///< Timestamp of the last request processed
	uint64_t number_of_requests; ///< Number of requests processed

	// Stats (These only get updated by `honas_state_persist()` when saving to disk)
	uint32_t estimated_number_of_clients;    ///< Estimated number of distinct clients
	uint32_t estimated_number_of_host_names; ///< Estimated number of distinct host names
	// followed by: uint32_t filter_bits_set[number_of_filters];
} __attribute__((packed));

/** Opened Honas state handle */
typedef struct {
	/* Cached information based on data from the header (pointers point inside the honas state `mmap()`-ed data) */
	struct honas_state_file_header* header;    ///< Honas state header
	byte_slice_t* filters;                     ///< Array of honas bloom filters
	uint32_t nr_filters_per_user_combinations; ///< Number of possible user filter combinations
	byte_slice_t client_count_registers;       ///< Hyperloglog data inside the honas state file to estimate number of distinct clients
	byte_slice_t host_name_count_registers;    ///< Hyperloglog data inside the honas state file to estimate number of distinct host names
	uint32_t* filter_bits_set;                 ///< References the sequence for `filter_bits_set` inside the honas state file header

	/* HyperLogLog states for client and host name cardinality estimation */
	hll client_count;    ///< Hyperloglog instance used to estimate the number of distinct clients
	hll host_name_count; ///< Hyperloglog instance used to estimate the number of distinct host names

	/* The actual honas state file data */
	void* mmap;  ///< The `mmap()`-ed honas state file
	size_t size; ///< The size of the `mmap()`-ed honas state file
} honas_state_t;

/** Create a new honas state
 *
 * \param state                      The honas state structure that is to be initialized
 * \param number_of_filters          Number of filters in the new honas state
 * \param number_of_bits_per_filter  Number of bits each of the filters
 * \param number_of_hashes           The number of hashes that should be set in each filter for every value
 * \param number_of_filters_per_user The number of filters that should be updated for each user
 * \param flatten_threshold          The threshold of estimated distinct clients below which the search results should be flattened for the given properties
 * \returns 0 on success or -1 on error (errno is set appropriately)
 * \ingroup honas_state
 */
extern int honas_state_create(honas_state_t* state, uint32_t number_of_filters, uint32_t number_of_bits_per_filter, uint32_t number_of_hashes, uint32_t number_of_filters_per_user, uint32_t flatten_threshold);

/** Load a honas state from a file
 *
 * \note When opening the honas state as `read-only` only the functions `honas_state_check_host_name_lookups()` and `honas_state_destroy()` may be called
 *
 * \param state     The honas state structure that is to be initialized
 * \param filename  The filename of the honas state on disk that should be loaded
 * \param read_only Whether the honas state should be opened read-only (and otherwise it will be opened for read-write)
 * \returns 0 on success or -1 on error (errno is set appropriately)
 * \ingroup honas_state
 */
extern int honas_state_load(honas_state_t* state, const char* filename, bool read_only);

/** Destroy the honas state
 *
 * This function should be called to release all resources associated with the
 * honas state to prevent resource leakage.
 *
 * \param state The honas state to be destroyed
 * \ingroup honas_state
 */
extern void honas_state_destroy(honas_state_t* state);

/** Register a host name lookup
 *
 * This is the function that `honas-gather` uses to register host name lookups in the honas state.
 *
 * The `host_name` and all parent domains (but not the TLDs) are hashed and the hashes are transformed
 * depending on which filter is to be updated. The filters being updated are selected based on th
 * value of `client`.
 *
 * \note The timestamp being passed in here is only used to update the statistics inside the honas
 *       state. No check is made to see if the timestamp is within the official period for this state.
 *
 * \param state            The honas state to update
 * \param timestamp        The timestamp of the host name lookup request
 * \param client           The client that made the host name lookup request
 * \param host_name        The host name that was being looked up
 * \param host_name_length The length of the host name that was being looked up
 * \ingroup honas_state
 */
extern void honas_state_register_host_name_lookup(honas_state_t* state, uint64_t timestamp, const struct in_addr46* client, const uint8_t* host_name, size_t host_name_length);

/** Check if the host name hash matches possible lookups
 *
 * This is the function that `honas-search` uses to check which host name lookups might be present in the honas state.
 *
 * \param state          The honas state to check
 * \param host_name_hash The hash of th host name that is to be looked up
 * \param filters_hit    Optional bitset that gets updated with which filters had possible hits
 * \returns Number of filters with possible hits
 * \ingroup honas_state
 */
extern uint32_t honas_state_check_host_name_lookups(honas_state_t* state, const byte_slice_t host_name_hash, bitset_t* filters_hit);

/** Save the honas state to file
 *
 * \param state    The honas state that is to be saved
 * \param filename The name of the file the state is to be saved to
 * \param blocking Whether the saving is to be done by this process (`true`)
 *                 or in a subprocess (`false`)
 * \ingroup honas_state
 */
extern void honas_state_persist(honas_state_t* state, const char* filename, bool blocking);

#endif /* HONAS_STATE_H */
