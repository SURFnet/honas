/*
 * A file input reader that attempts to find items separated by some
 * specific character in a fixed size buffer of bytes.
 *
 * This implementation works correctly with short reads due to signals,
 * which apparently causes 'getline' and 'getdelim' to sometimes return
 * partial items.
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

#ifndef DELIM_READER_H
#define DELIM_READER_H

#include "includes.h"

/// \defgroup delim_reader Reading from a byte delimited file

/** Byte deliminated file reader
 */
typedef struct {
	/** The read buffer
	 * \private
	 */
	char* buf;
	/** The size of the read buffer (the `max_item_len` from `delim_reader_init()`)
	 * \private
	 */
	size_t len;

	/** The offset of the next item inside the read buffer
	 * \private
	 */
	size_t off;
	/** The number of bytes present in the read buffer
	 * \private
	 */
	size_t data;

	/** The filedescriptor that is to be read from
	 * \private
	 */
	int fd;
	/** The separator that deliminates the items
	 * \private
	 */
	char sep;
} delim_reader_t;

/** Initialize a delimited reader structure
 *
 * The items being looked for should end with the 'sep' character and the maximum item size
 * being looked for is 'max_item_len', which is also the size of the ring buffer.
 *
 * \note For I/O efficiency it's better to use a larger 'max_item_len' and
 *       check the returned item sizes for input sanitation.
 *
 * \remark Using '\n' as a deliminator effectively makes this a line reader.
 *
 * \param ctx          The reader context that is to be initialized
 * \param fd           The filedescriptor that should be read from
 * \param sep          The character that is used to deliminate all items
 * \param max_item_len The maximum size of a single item
 * \ingroup delim_reader
 */
extern int delim_reader_init(delim_reader_t* ctx, int fd, char sep, size_t max_item_len);

/** Cleanup reader
 *
 * This function cleans up all resources acquired during the call
 * to `delim_reader_create()` and should be called when the caller
 * is done with the reader to prevent resource leakage.
 *
 * \remark This doesn't close the underlying filedescriptor
 *
 * \param ctx The reader that is to be destroyed
 * \ingroup delim_reader
 */
extern void delim_reader_destroy(delim_reader_t* ctx);

/** Try and find the next item in the buffer, reading more data when necessary
 *
 * If an item is found than the 'rbuf' pointer is pointed at the locaton in the buffer where
 * the item begins. The return value indicates the size of the item.
 *
 * The caller is free to do anything with the returned item data (beginning at 'rbuf' and
 * ending at 'rbuf' + return length) up until the next call of this function or until
 * the delim_reader_destroy() function is called.
 *
 * \warning Any access outside the returned data results in undefined behaviour.
 *
 * \param ctx The reader that is used to read items from
 * \param rbuf A pointer that gets updated to point at the pointer of the next item inside the read buffer
 * \returns On success the number of bytes the item is long,
 *          -1 on I/O errors (errno is set appriopriately)
 *          or -2 when the buffer was full en still no item was found
 * \ingroup delim_reader
 */
extern ssize_t delim_reader_next(delim_reader_t* ctx, char** rbuf);

#endif /* DELIM_READER_H */
