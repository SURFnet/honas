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

#include "honas_state.h"
#include "bitset.h"
#include "bloom.h"
#include "combinations.h"
#include "logging.h"

#include <ctype.h>
#include <openssl/sha.h>

#if BYTE_ORDER != LITTLE_ENDIAN
#error The currrent implementation only works properly on a little endian machine!
#endif

static size_t round_up_to_factor_of_two(size_t value, size_t factor_of_two)
{
	return (value + (1 << factor_of_two) - 1) & (~((1 << factor_of_two) - 1));
}

static uint64_t uint64_hash(const byte_slice_t data)
{
	return byte_slice_MurmurHash64A(data, 0xadc83b19ULL);
}

static size_t honas_state_file_size(uint32_t first_filter_offset, uint32_t padding_after_filters, uint32_t number_of_filters, uint32_t number_of_bits_per_filter, uint32_t client_hll_size, uint32_t padding_after_client_hll, uint32_t host_name_hll_size, uint32_t padding_after_host_name_hll)
{
	assert(number_of_filters > 0);
	assert(number_of_bits_per_filter > 0);
	assert((number_of_bits_per_filter & 0x7) == 0);
	assert(first_filter_offset >= sizeof(struct honas_state_file_header));

	return first_filter_offset
		+ number_of_filters * (number_of_bits_per_filter >> 3)
		+ number_of_filters * padding_after_filters
		+ client_hll_size + padding_after_client_hll
		+ host_name_hll_size + padding_after_host_name_hll;
}

static void honas_state_init_common(honas_state_t* state)
{
	assert(state->mmap != NULL);
	assert(state->size >= sizeof(struct honas_state_file_header));
	assert(state->header != NULL);
	assert(state->filters == NULL);

//	log_msg(ERR, "First filter offset: %zu, padding after filters: %zu, number of filters: %zu, size (m): %zu, client hll size: %zu, hll padding: %zu, hostname_hll_size: %zu, padding_after_hostname_hll: %zu. state size: %zu, all together: %zu",
//		state->header->first_filter_offset, state->header->padding_after_filters, state->header->number_of_filters, state->header->number_of_bits_per_filter, state->header->client_hll_size, state->header->padding_after_client_hll,
//		state->header->host_name_hll_size, state->header->padding_after_host_name_hll, state->size , honas_state_file_size(state->header->first_filter_offset, state->header->padding_after_filters, state->header->number_of_filters,
//                                                          state->header->number_of_bits_per_filter, state->header->client_hll_size, state->header->padding_after_client_hll, state->header->host_name_hll_size, state->header->padding_after_host_name_hll));

	assert(state->size >= honas_state_file_size(state->header->first_filter_offset, state->header->padding_after_filters, state->header->number_of_filters, state->header->number_of_bits_per_filter, state->header->client_hll_size,
							  state->header->padding_after_client_hll, state->header->host_name_hll_size, state->header->padding_after_host_name_hll));

	state->filters = (byte_slice_t*)calloc(state->header->number_of_filters, sizeof(byte_slice_t));
	log_passert(state->filters != NULL, "Failed to allocation honas state filters");

	for (uint32_t i = 0; i < state->header->number_of_filters; i++) {
		size_t filter_begin = state->header->first_filter_offset
			+ i * state->header->padding_after_filters
			+ i * (state->header->number_of_bits_per_filter >> 3);
		assert((filter_begin + (state->header->number_of_bits_per_filter >> 3)) <= state->size);
		state->filters[i] = byte_slice((uint8_t*)state->mmap + filter_begin, state->header->number_of_bits_per_filter >> 3);
	}
	state->client_count_registers = byte_slice((uint8_t*)state->mmap + ((state->header->number_of_bits_per_filter >> 3) + state->header->padding_after_filters) * state->header->number_of_filters, state->header->client_hll_size);
	state->host_name_count_registers = byte_slice((uint8_t*)state->client_count_registers.bytes + state->header->client_hll_size + state->header->padding_after_client_hll, state->header->host_name_hll_size);
	state->nr_filters_per_user_combinations = number_of_combinations(state->header->number_of_filters, state->header->number_of_filters_per_user);
	state->filter_bits_set = (uint32_t*)((uint8_t*)state->mmap + sizeof(struct honas_state_file_header));
}

int honas_state_create(honas_state_t* state, uint32_t number_of_filters, uint32_t number_of_bits_per_filter, uint32_t number_of_hashes, uint32_t number_of_filters_per_user, uint32_t flatten_threshold)
{
	assert(state->mmap == NULL);
	assert(state->header == NULL);
	assert(state->filters == NULL);
	assert(number_of_filters > 0);
	assert(number_of_bits_per_filter > 0);
	assert((number_of_bits_per_filter & 0x7) == 0);
	assert(number_of_hashes > 0);
	assert(number_of_filters_per_user > 0);
	int saved_errno;
	int err_return = -1;

	/* Make sure the filters begin on new page after the state file header */
	uint32_t first_filter_offset = round_up_to_factor_of_two(sizeof(struct honas_state_file_header) + sizeof(uint32_t) * number_of_filters, PAGE_SHIFT);

	/* Make sure each filter starts on a new page after the previous one */
	uint32_t padding_after_filters = round_up_to_factor_of_two(number_of_bits_per_filter >> 3, PAGE_SHIFT) - (number_of_bits_per_filter >> 3);
	uint32_t padding_after_client_hll = round_up_to_factor_of_two(HLL_DENSE_SIZE, PAGE_SHIFT) - HLL_DENSE_SIZE;
	uint32_t padding_after_host_name_hll = round_up_to_factor_of_two(HLL_DENSE_SIZE, PAGE_SHIFT) - HLL_DENSE_SIZE;

	state->size = honas_state_file_size(first_filter_offset, padding_after_filters, number_of_filters, number_of_bits_per_filter, HLL_DENSE_SIZE, padding_after_client_hll, HLL_DENSE_SIZE, padding_after_host_name_hll);
	if ((state->mmap = mmap(NULL, state->size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
		goto err_out;

	state->header = (struct honas_state_file_header*)state->mmap;
	memcpy(state->header->file_magic, HONAS_STATE_FILE_MAGIC, sizeof(state->header->file_magic));
	state->header->major_version = CURRENT_HONAS_STATE_MAJOR_VERSION;
	state->header->minor_version = CURRENT_HONAS_STATE_MINOR_VERSION;

	state->header->first_filter_offset = first_filter_offset;
	state->header->padding_after_filters = padding_after_filters;
	state->header->number_of_filters = number_of_filters;
	state->header->number_of_bits_per_filter = number_of_bits_per_filter;
	state->header->number_of_hashes = number_of_hashes;
	state->header->number_of_filters_per_user = number_of_filters_per_user;
	state->header->flatten_threshold = flatten_threshold;

	state->header->client_hll_size = HLL_DENSE_SIZE;
	state->header->padding_after_client_hll = padding_after_client_hll;
	state->header->host_name_hll_size = HLL_DENSE_SIZE;
	state->header->padding_after_host_name_hll = padding_after_host_name_hll;

	hllInit(&state->client_count);
	hllInit(&state->host_name_count);

	honas_state_init_common(state);
	return 0;

err_out:
	saved_errno = errno;
	honas_state_destroy(state);
	errno = saved_errno;
	return err_return;
}

int honas_state_load(honas_state_t* state, const char* filename, bool read_only)
{
	assert(state->mmap == NULL);
	assert(state->header == NULL);
	assert(state->filters == NULL);
	int saved_errno;
	int err_return = -1;

	/* attempt to open state file */
	int fd = open(filename, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		goto err_out;

	{ /* determine state file size */
		struct stat fd_stat;
		if (fstat(fd, &fd_stat) == -1)
			goto err_out;
		state->size = fd_stat.st_size;
	}
	if ((state->mmap = mmap(NULL, state->size, (read_only ? PROT_READ : PROT_READ | PROT_WRITE), MAP_PRIVATE, fd, 0)) == MAP_FAILED)
		goto err_out;
	if (!read_only && mlock(state->mmap, state->size) == -1)
		log_perror(INFO, "Unable to mlock honas state");
	if (close(fd) == -1)
		log_perror(ERR, "Error closing loaded state file '%s'", filename);
	fd = -1;

	/* Check state file compatibility */
	state->header = (struct honas_state_file_header*)state->mmap;
	if (
		state->size < 44 /* part of state that describes: file magic, versioning and info needed to calculate the filter size */
		|| memcmp(state->header->file_magic, HONAS_STATE_FILE_MAGIC, sizeof(state->header->file_magic)) != 0
		|| state->header->major_version != CURRENT_HONAS_STATE_MAJOR_VERSION) {
		err_return = 1;
		goto err_out;
	}

	/* Verify basic state file information */
	if (
		state->header->first_filter_offset < sizeof(struct honas_state_file_header)
		|| state->header->number_of_filters == 0
		|| state->header->number_of_bits_per_filter == 0
		|| (state->header->number_of_bits_per_filter & 0x7) != 0
		|| state->header->number_of_hashes == 0
		|| state->header->client_hll_size != ((uint32_t)HLL_DENSE_SIZE)
		|| state->header->host_name_hll_size != ((uint32_t)HLL_DENSE_SIZE)
		|| state->size < honas_state_file_size(
							 state->header->first_filter_offset,
							 state->header->padding_after_filters,
							 state->header->number_of_filters,
							 state->header->number_of_bits_per_filter,
							 state->header->client_hll_size,
							 state->header->padding_after_client_hll,
							 state->header->host_name_hll_size,
							 state->header->padding_after_host_name_hll)) {
		err_return = 2;
		goto err_out;
	}

	honas_state_init_common(state);
	hllInitFromBuffer(&state->client_count, state->client_count_registers);
	hllInitFromBuffer(&state->host_name_count, state->host_name_count_registers);
	return 0;

err_out:
	saved_errno = errno;
	if (fd != -1)
		close(fd);
	honas_state_destroy(state);
	errno = saved_errno;
	return err_return;
}

/*
 * This function generates a stable (per filter index) derivation of the host_name_hash.
 *
 * This should ensure that the same hostname uses different bloom filter offsets in different bloom filters,
 * which in turn should lead to fewer false positives when multiple bloom filter results are combined.
 */
static void filter_index_host_name_hash_transform(uint32_t filter_index, const byte_slice_t src, byte_slice_t dst)
{
	assert(src.len == dst.len);
	if (filter_index == 0) {
		memcpy(dst.bytes, src.bytes, dst.len);
	} else {
		assert(dst.len % sizeof(uint64_t) == 0);
		uint64_t multiplier = filter_index * 2 + 1;
		uint64_t *from = (uint64_t*) src.bytes, *to = (uint64_t*) dst.bytes;
		size_t len = dst.len / sizeof(uint64_t);
		for (size_t i = 0; i < len; i++)
			to[i] = from[i] * multiplier;
	}
}

void honas_state_register_host_name_lookup(honas_state_t* state, uint64_t timestamp, const struct in_addr46* client, const uint8_t* host_name, size_t host_name_length
	, const uint8_t* entity_prefix, size_t entity_prefix_length, struct dry_run_counters* p_dryrun, const ldns_rr_type qtype)
{
	/* Check if more than a second has passed since the previous request */
	if (state->header->last_request < timestamp) {
		state->header->last_request = timestamp;

		/* Check if timestamp of this was the first request in this period */
		if (state->header->first_request == 0)
			state->header->first_request = timestamp;
	}

	/* Count the request */
	state->header->number_of_requests++;

	/* Determine client hash used for filter selection and counting the client */
	uint64_t client_hash;
	if (client->af == AF_INET) {
		client_hash = uint64_hash(byte_slice_from_scalar(client->in.addr4.s_addr));
	} else if (client->af == AF_INET6) {
		client_hash = uint64_hash(byte_slice_from_scalar(client->in.addr6.s6_addr));
	} else {
		log_die("Unsupported address family '%d'", client->af);
	}

	/* Count client */
	hllAdd(&state->client_count, client_hash);

	/* Lookup filter information */
	byte_slice_t* filters = state->filters;
	uint32_t nr_filters = state->header->number_of_filters;
	uint32_t nr_hashes = state->header->number_of_hashes;
	uint32_t nr_filters_per_user = state->header->number_of_filters_per_user;

	/* Determine which filters to use for this client */
	uint32_t filter_indexes[nr_filters_per_user];
	uint32_t combination = client_hash % state->nr_filters_per_user_combinations;
	lookup_combination(nr_filters, filter_indexes, nr_filters_per_user, combination);

	/* Ignore possible trailing '.' */
	if (host_name[host_name_length - 1] == '.')
		host_name_length--;

	// Canonicalize the domain name.
	uint8_t local_host_name[256] = { 0 };
	for (size_t i = 0; i < host_name_length; ++i)
	{
		local_host_name[i] = tolower(host_name[i]);
	}

	uint8_t host_name_hash[SHA256_DIGEST_LENGTH], transformed_host_name_hash[SHA256_DIGEST_LENGTH];
	byte_slice_t host_name_hash_slice = byte_slice_from_array(host_name_hash);
	byte_slice_t transformed_host_name_hash_slice = byte_slice_from_array(transformed_host_name_hash);
	const uint8_t *part_start = local_host_name, *part_end = local_host_name + host_name_length, *part_next;
	uint8_t localbuf[512];
	uint8_t sld_buf[256] = { 0 };

	// Add the domain name as a whole, excluding the entity prefix.
	SHA256(local_host_name, host_name_length, host_name_hash_slice.bytes);

	/* Count host name */
	assert(SHA256_DIGEST_LENGTH >= sizeof(uint64_t));
	hllAdd(&state->host_name_count, byte_slice_as_uint64_ptr(host_name_hash_slice)[0]);

	/* Register host name in filters */
	for (uint32_t i = 0; i < nr_filters_per_user; i++)
	{
		uint32_t filter_index = filter_indexes[i];
		filter_index_host_name_hash_transform(filter_index, host_name_hash_slice, transformed_host_name_hash_slice);
		bloom_set(filters[filter_index], transformed_host_name_hash_slice, nr_hashes);
	}

	// Add to dry-run parameters.
	if (p_dryrun)
	{
		hllAdd(&p_dryrun->hourly_global, uint64_hash(byte_slice(local_host_name, host_name_length)));
		hllAdd(&p_dryrun->daily_global, uint64_hash(byte_slice(local_host_name, host_name_length)));
	}

	// If present, prepend the entity name to the whole domain name.
	if (entity_prefix)
	{
		// Copy the entity name as prefix to the local buffer and add a separator.
		strncpy((char*)localbuf, (char*)entity_prefix, sizeof(localbuf));
		localbuf[strlen((char*)localbuf)] = '@';

		// Append the domain name label.
		strncat((char*)localbuf, (char*)local_host_name, sizeof(localbuf) - strlen((char*)localbuf));

		/* Calculate the hash of the label */
		SHA256(localbuf, strlen((char*)localbuf), host_name_hash_slice.bytes);

		/* Count host name */
		assert(SHA256_DIGEST_LENGTH >= sizeof(uint64_t));
		hllAdd(&state->host_name_count, byte_slice_as_uint64_ptr(host_name_hash_slice)[0]);

		// Add to dry-run parameters.
		if (p_dryrun)
		{
			hllAdd(&p_dryrun->hourly_global, byte_slice_as_uint64_ptr(host_name_hash_slice)[0]);
			hllAdd(&p_dryrun->daily_global, byte_slice_as_uint64_ptr(host_name_hash_slice)[0]);
		}

		/* Register host name in filters */
		for (uint32_t i = 0; i < nr_filters_per_user; i++)
		{
			uint32_t filter_index = filter_indexes[i];
			filter_index_host_name_hash_transform(filter_index, host_name_hash_slice, transformed_host_name_hash_slice);
			bloom_set(filters[filter_index], transformed_host_name_hash_slice, nr_hashes);
		}
	}

	// Check which record type we are dealing with. If it is a PTR record, we don't want to store the separate labels.
	if (qtype != LDNS_RR_TYPE_PTR)
	{
		/* Process the host name parts */
		/* Add hashes for all subdomains, except for the tld (so the part must always include at least one '.') */
		while ((part_next = memchr(part_start, '.', part_end - part_start)) != NULL)
		{
			// Check if an entity name was specified.
			if (entity_prefix)
			{
				// Copy the entity name as prefix to the local buffer and add a separator.
				strncpy((char*)localbuf, (char*)entity_prefix, sizeof(localbuf));
				localbuf[strlen((char*)localbuf)] = '@';

				// Append the domain name label.
				strncat((char*)localbuf, (char*)part_start, part_next - part_start);

				/* Calculate the hash of the label */
				SHA256(localbuf, strlen((char*)localbuf), host_name_hash_slice.bytes);

				/* Count host name */
				assert(SHA256_DIGEST_LENGTH >= sizeof(uint64_t));
				hllAdd(&state->host_name_count, byte_slice_as_uint64_ptr(host_name_hash_slice)[0]);

				// Add to dry-run parameters.
				if (p_dryrun)
				{
					hllAdd(&p_dryrun->hourly_global, byte_slice_as_uint64_ptr(host_name_hash_slice)[0]);
					hllAdd(&p_dryrun->daily_global, byte_slice_as_uint64_ptr(host_name_hash_slice)[0]);
				}

				/* Register host name in filters */
				for (uint32_t i = 0; i < nr_filters_per_user; i++)
				{
					uint32_t filter_index = filter_indexes[i];
					filter_index_host_name_hash_transform(filter_index, host_name_hash_slice, transformed_host_name_hash_slice);
					bloom_set(filters[filter_index], transformed_host_name_hash_slice, nr_hashes);
				}
			}

			/* Calculate the hash of the label */
			SHA256(part_start, part_next - part_start, host_name_hash_slice.bytes);

			/* Count host name */
			assert(SHA256_DIGEST_LENGTH >= sizeof(uint64_t));
			hllAdd(&state->host_name_count, byte_slice_as_uint64_ptr(host_name_hash_slice)[0]);

			// Add to dry-run parameters.
			if (p_dryrun)
			{
				hllAdd(&p_dryrun->hourly_global, byte_slice_as_uint64_ptr(host_name_hash_slice)[0]);
				hllAdd(&p_dryrun->daily_global, byte_slice_as_uint64_ptr(host_name_hash_slice)[0]);
			}

			/* Register host name in filters */
			for (uint32_t i = 0; i < nr_filters_per_user; i++)
			{
				uint32_t filter_index = filter_indexes[i];
				filter_index_host_name_hash_transform(filter_index, host_name_hash_slice, transformed_host_name_hash_slice);
				bloom_set(filters[filter_index], transformed_host_name_hash_slice, nr_hashes);
			}

			// Copy the current label to a separate buffer, so that we can take out the SLD in the end.
			strncpy((char*)sld_buf, (char*)part_start, part_end - part_start);
			sld_buf[part_end - part_start] = 0;

			/* Check for next part */
			part_start = part_next + 1; /* +1 as the next part actually starts after the '.' */
		}

		// Add the SLD.TLD to the Bloom filter as well.
		SHA256(sld_buf, strlen((char*)sld_buf), host_name_hash_slice.bytes);

		/* Count host name */
		assert(SHA256_DIGEST_LENGTH >= sizeof(uint64_t));
		hllAdd(&state->host_name_count, byte_slice_as_uint64_ptr(host_name_hash_slice)[0]);

		// Add to dry-run parameters.
		if (p_dryrun)
		{
			hllAdd(&p_dryrun->hourly_global, byte_slice_as_uint64_ptr(host_name_hash_slice)[0]);
			hllAdd(&p_dryrun->daily_global, byte_slice_as_uint64_ptr(host_name_hash_slice)[0]);

			// Update total query counters.
			++p_dryrun->hourly_total_queries;
			++p_dryrun->daily_total_queries;
		}

		/* Register host name in filters */
		for (uint32_t i = 0; i < nr_filters_per_user; i++)
		{
			uint32_t filter_index = filter_indexes[i];
			filter_index_host_name_hash_transform(filter_index, host_name_hash_slice, transformed_host_name_hash_slice);
			bloom_set(filters[filter_index], transformed_host_name_hash_slice, nr_hashes);
		}
	}
}

uint32_t honas_state_check_host_name_lookups(honas_state_t* state, const byte_slice_t host_name_hash, bitset_t* filters_hit) {
	/* Lookup filter information */
	byte_slice_t* filters = state->filters;
	uint32_t nr_filters = state->header->number_of_filters;
	uint32_t nr_hashes = state->header->number_of_hashes;

	/* Count the filters that probably contain the host name */
	uint32_t filter_count = 0;
	uint8_t transformed_host_name_hash[host_name_hash.len];
	for (uint32_t i = 0; i < nr_filters; i++) {
		filter_index_host_name_hash_transform(i, host_name_hash, byte_slice_from_array(transformed_host_name_hash));
		if (bloom_is_set(filters[i], byte_slice_from_array(transformed_host_name_hash), nr_hashes)) {
			filter_count++;
			if (filters_hit != NULL)
				bitset_set_bit(filters_hit, i);
		}
	}
	return filter_count;
}

void honas_state_persist(honas_state_t* state, const char* filename, bool blocking)
{
	if (!blocking) {
		/* Perform save to disk in a child process so as not to block the main process during this possibly slow and intensive operation */
		switch (fork()) {
		case -1:
			log_pfail("Unable to save honas state, failed to fork");
		case 0:
			/* Child process; This process should execute the remainder of this function */
			break;
		default:
			/* Parent process; This process should skip the remainder of this function */
			return;
		}
	}

	/* Make sure all hyperloglog data is dense and present in the state file */
	if (state->client_count.registers_owned) {
		hllSparseToDense(&state->client_count);
		byte_slice_bitwise_or(state->client_count_registers, state->client_count.registers);
		hllDestroy(&state->client_count);
		hllInitFromBuffer(&state->client_count, state->client_count_registers);
	}
	state->header->estimated_number_of_clients = hllCount(&state->client_count, NULL);

	if (state->host_name_count.registers_owned) {
		hllSparseToDense(&state->host_name_count);
		byte_slice_bitwise_or(state->host_name_count_registers, state->host_name_count.registers);
		hllDestroy(&state->host_name_count);
		hllInitFromBuffer(&state->host_name_count, state->host_name_count_registers);
	}
	state->header->estimated_number_of_host_names = hllCount(&state->host_name_count, NULL);

	/* Count the number of filter bits set in each filter */
	for (uint32_t i = 0; i < state->header->number_of_filters; i++)
		state->filter_bits_set[i] = bloom_nr_bits_set(state->filters[i]);

	/* Create a preallocated tempfile */
	int fd = open(".", O_TMPFILE | O_RDWR | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP);
	log_passert(fd != -1, "Unable to save honas state to '%s', failed to open file", filename);
	log_passert(fallocate(fd, 0, 0, state->size) != -1, "Unable to save honas state to '%s', failed to preallocat file", filename);

	/* Write the state data to the tempfile */
	size_t total_written = 0;
	while (total_written < state->size) {
		ssize_t written = write(fd, ((uint8_t*)state->mmap) + total_written, state->size - total_written);
		log_passert(written != -1, "Unable to save honas state to '%s', failed to write", filename);
		total_written += written;
	}

	/* Release the honas state resources after writing them to file in the child process */
	if (!blocking)
		honas_state_destroy(state);

	/* Create a link from tempfile to the indicated filename
	 * This will probably trigger an fsync on the file data and could block for a longer period of time
	 */
	char fdpath[PATH_MAX];
	snprintf(fdpath, PATH_MAX, "/proc/self/fd/%d", fd);
	log_passert(linkat(AT_FDCWD, fdpath, AT_FDCWD, filename, AT_SYMLINK_FOLLOW) != -1, "Unable to save honas state to '%s', failed to linkat", filename);
	log_passert(close(fd) != -1, "Unable to save honas state to '%s', failed to close", filename);

	/* Exit the state saving child process */
	if (!blocking) {
		log_destroy();
		exit(0);
	}
}

void honas_state_destroy(honas_state_t* state)
{
	if (state->client_count.registers.bytes != NULL)
		hllDestroy(&state->client_count);
	if (state->host_name_count.registers.bytes != NULL)
		hllDestroy(&state->host_name_count);
	if (state->filters != NULL) {
		free(state->filters);
		state->filters = NULL;
	}
	if (state->header != NULL)
		state->header = NULL;
	if (state->mmap != NULL) {
		if (state->mmap != MAP_FAILED && munmap(state->mmap, state->size) == -1)
			log_perror(ERR, "Failed to unmap honas state");
		state->mmap = NULL;
		state->size = 0;
	}
}

// NOTE: This function assumes that the order of the Bloom filters in each state file is the same!
// For example: If the seed for the Bloom filters is a sequence number, the sequence number must
// be applied in the same order in both target and source.
const bool honas_state_aggregate_combine(honas_state_t* target, honas_state_t* source)
{
	// Check whether the pointers are valid.
	if (target && source)
	{
		// Check whether the parameters k and m are the same, and if the state files both
		// contain the same number of filters.
		if (target->header->number_of_bits_per_filter == source->header->number_of_bits_per_filter
			&& target->header->number_of_hashes == source->header->number_of_hashes
			&& target->header->number_of_filters == source->header->number_of_filters)
		{
			// Loop over all filters in the target state.
			for (size_t i = 0; i < target->header->number_of_filters; ++i)
			{
				// Take the bitwise OR of the target and source Bloom filter.
				byte_slice_bitwise_or(target->filters[i], source->filters[i]);

				// Merge the HyperLogLog structure for client count in both states.
				hllMerge(&target->client_count, &source->client_count);

				// Merge the HyperLogLog structure for host name count in both states.
				hllMerge(&target->host_name_count, &source->host_name_count);
			}

			// The operation succeeded.
			return true;
		}

		// The parameters or filter count are invalid.
		return false;
	}

	// The pointers are invalid.
	return false;
}

// Calculates minimum required bits of entropy for a Bloom filter.
const uint32_t honas_state_calculate_required_entropy(honas_state_t* state)
{
	if (state)
	{
		// Calculated with k * ceil(log_2(m)).
		return state->header->number_of_hashes * ceil(log2(state->header->number_of_bits_per_filter));
	}

	// Invalid state parameter specified.
	return 0;
}
