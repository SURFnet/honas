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

#include "delim_reader.h"

int delim_reader_init(delim_reader_t* ctx, int fd, char sep, size_t max_item_len)
{
	ctx->buf = malloc(max_item_len);
	if (ctx->buf == NULL)
		return -1;
	ctx->len = max_item_len;
	ctx->off = 0;
	ctx->data = 0;
	ctx->fd = fd;
	ctx->sep = sep;
	return 0;
}

void delim_reader_destroy(delim_reader_t* ctx)
{
	if (ctx->buf != NULL) {
		free(ctx->buf);
		ctx->buf = NULL;
	}
}

ssize_t delim_reader_next(delim_reader_t* ctx, char** rbuf)
{
	ssize_t rd;

	while (1) {
		/* Check if an item is present in the ring buffer */
		if (ctx->data > 0) {
			/* Look for separator */
			char* sep = memchr(ctx->buf + ctx->off, ctx->sep, ctx->data);
			if (sep != NULL) {
				/* Found item */
				size_t len = sep - ctx->buf - ctx->off + 1;
				*rbuf = ctx->buf + ctx->off;

				/* "Remove" data from buffer */
				ctx->off += len;
				ctx->data -= len;

				return len;
			}
		}

		/* Return error if the buffer is full (as nothing was found) and flush buffer */
		if (ctx->off == 0 && ctx->data == ctx->len) {
			ctx->data = 0;
			return -2;
		}

		/* Move remaining data to the front of the buffer */
		if (ctx->data > 0)
			memmove(ctx->buf, ctx->buf + ctx->off, ctx->data);
		ctx->off = 0;

		/* Read more data */
		rd = read(ctx->fd, ctx->buf + ctx->data, ctx->len - ctx->data);
		if (rd > 0)
			ctx->data += rd;
		else
			return rd;
	}
}
