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

#include "utils.h"
#include "includes.h"

int my_strtol(const char* str, int* val)
{
	char* endptr;
	errno = 0;
	int v = strtol(str, &endptr, 10);
	if (errno != 0)
		return -1; // parse error
	if (str == endptr)
		return -1; // no numbers found
	if (*endptr != 0)
		return -1; // could not parse whole string
	*val = v;
	return 0;
}

bool my_strtouint16(const char* str, uint16_t* val, char** supplied_endptr, int base)
{
	char* default_endptr;
	char** endptr = supplied_endptr == NULL ? &default_endptr : supplied_endptr;
	errno = 0;
	unsigned long int v = strtoul(str, endptr, base);
	if (errno != 0)
		return false; // parse error
	if (str == *endptr)
		return false; // no numbers found
	if (v > UINT16_MAX)
		return false; // parsed number too big
	if (supplied_endptr == NULL && *default_endptr != '\0')
		return false; // could not parse whole string
	*val = v;
	return true;
}

bool my_strtouint32(const char* str, uint32_t* val, char** supplied_endptr, int base)
{
	char* default_endptr;
	char** endptr = supplied_endptr == NULL ? &default_endptr : supplied_endptr;
	errno = 0;
	unsigned long int v = strtoul(str, endptr, base);
	if (errno != 0)
		return false; // parse error
	if (str == *endptr)
		return false; // no numbers found
	if (v > UINT32_MAX)
		return false; // parsed number too big
	if (supplied_endptr == NULL && *default_endptr != '\0')
		return false; // could not parse whole string
	*val = v;
	return true;
}

bool my_strtouint64(const char* str, uint64_t* val, char** supplied_endptr, int base)
{
	char* default_endptr;
	char** endptr = supplied_endptr == NULL ? &default_endptr : supplied_endptr;
	errno = 0;
#if __WORDSIZE == 64
	unsigned long int v = strtoul(str, endptr, base);
#else
	unsigned long long int v = strtoull(str, endptr, base);
#endif
	if (errno != 0)
		return false; // parse error
	if (str == *endptr)
		return false; // no numbers found
	if (v > UINT64_MAX)
		return false; // parsed number too big
	if (supplied_endptr == NULL && *default_endptr != '\0')
		return false; // could not parse whole string
	*val = v;
	return true;
}

char* create_relative_filepath(const char* orig_file, const char* rel_file)
{
	char* idx = rindex(orig_file, '/');
	if (idx == NULL)
		return strdup(rel_file);

	size_t orig_file_len = idx - orig_file + 1;
	size_t rel_file_len = strlen(rel_file);
	size_t file_len = orig_file_len + rel_file_len;

	char* file = malloc(file_len + 1);
	if (file == NULL)
		return NULL;

	memcpy(file, orig_file, orig_file_len);
	memcpy(file + orig_file_len, rel_file, rel_file_len);
	file[file_len] = 0;

	return file;
}

char* index_ws(const char* s)
{
	char* idx_spc = index(s, ' ');
	char* idx_tab = index(s, '\t');
	if (idx_spc == NULL)
		return idx_tab;
	if (idx_tab == NULL)
		return idx_spc;
	if (idx_spc < idx_tab)
		return idx_spc;
	return idx_tab;
}

int cmpstringp(const void* p1, const void* p2)
{
	return strcmp(*(char* const*)p1, *(char* const*)p2);
}

