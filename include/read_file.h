/*
 * Copyright (c) 2008-2017, Quarantainenet Holding B.V.
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

/** Read a file line-by-line. Supports on the fly decompression of files.
 *
 * This is a slightly cleaned-up version of 'read_file.h' from the qnet-dns-relayd.
 */

#ifndef READ_FILE_H
#define READ_FILE_H

#define MAX_FILE_LINE_LENGTH 16384

/** Callback function that gets called for each line
 *
 * \param filename The name of the file being processed
 * \param ptr      An opaque pointer that can be used to pass state to the callback function
 * \param lineno   The line number of the line that was read
 * \param line     The line that was read without the newline character and nul-terminated
 * \param length   The length of the line that was read
 */
typedef void(parse_line_t)(const char* filename, void* ptr, unsigned int lineno, char* line, unsigned int length);

/** Read a file line by line
 *
 * \param filename   The file to read
 * \param ptr        An opaque pointer passed to the per line callback function
 * \param parse_line A pointer to a callback function that gets called for each read line
 * \returns 0 on success,
 *          -1 on I/O error (errno is set appropriately),
 *          -2 unexpected behaviour of `read()`,
 *          -3 when the file didn't end with a newline,
 *          -4 line too long or
 *          -5 decompression error
 */
extern int read_file(const char* filename, void* ptr, parse_line_t* parse_line);

#endif // READ_FILE_H
