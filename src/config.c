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

/*
 * This file contains a simple configuration file parser.
 *
 * It's loosely based on the qnet-dns-relayd config parser.
 */

#include "config.h"
#include "logging.h"
#include "read_file.h"
#include "utils.h"

struct config_read_context {
	int config_include_loop; // detection for config include-loops..
	void* ptr;
	parse_item_t* parse_item;
};

static void _config_read(const char* filename, struct config_read_context* ctx);

static void
config_parse_line(const char* filename, struct config_read_context* ctx, unsigned int lineno, char* line, unsigned int UNUSED(length))
{
	// skip leading spaces
	while ((*line == ' ') || (*line == '\t'))
		line = &line[1];

	if (*line == 0)
		return; // end of line

	if ((*line == '#') || (*line == ';'))
		return; // comment

	char* keyword = line;
	while (((*line >= '0') && (*line <= '9'))
		|| ((*line >= 'A') && (*line <= 'Z'))
		|| ((*line >= 'a') && (*line <= 'z'))
		|| (*line == '_')) {
		line = &line[1];
	}

	if ((keyword == line) || ((*line != ' ') && (*line != '\t') && (*line != 0))) {
		log_die("[%s:%u]  Keyword missing: '%s'", filename, lineno, line);
		return; // Quelch unused variable warnings
	}

	if (*line != 0) {
		*line = 0;
		line = &line[1];
	}

	// skip spaces
	while ((*line == ' ') || (*line == '\t'))
		line = &line[1];

	// strip trailing spaces
	int len = strlen(line);
	while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t'))
		len--;
	line[len] = 0;

	/* process configuration directives */
	if (strcmp((char*)keyword, "include") == 0) {
		// include file 'line'
		if (ctx->config_include_loop++ > 10)
			log_die("[%s:%u]  Reached max. include depth", filename, lineno);

		int if_exist = 0;

		char* str_action = index_ws(line);
		if (str_action != NULL) {
			*str_action = 0;
			str_action = &str_action[1];
			while (*str_action == ' ' || *str_action == '\t') {
				str_action = &str_action[1];
			}

			if (strcmp(str_action, "if_exist") == 0)
				if_exist++;
			else
				log_die("[%s:%u]  Syntax error in include line: '%s'", filename, lineno, line);
		}

		char* newfile;
		if (*line == '/') {
			newfile = line;
		} else {
			newfile = create_relative_filepath(filename, line);
			log_passert(newfile != NULL, "Failed to allocate memory");
		}

		if (if_exist == 0 || (access(newfile, R_OK) == 0)) {
			struct stat st;
			int err = stat(newfile, &st);
			log_passert(err != -1, "stat()");
			if (S_ISREG(st.st_mode)) {
				_config_read(newfile, ctx);
			} else if (S_ISDIR(st.st_mode)) {
				char* dir = alloca(strlen(newfile) + 2);
				memcpy(dir, newfile, strlen(newfile) + 1);
				if (strlen(dir) == 0 || dir[strlen(dir) - 1] != '/')
					strcat(dir, "/");
				DIR* d;
				d = opendir(dir);
				log_passert(d != NULL, "opendir(%s)", newfile);
				char** list = NULL;
				size_t list_size = 0;
				while (1) {
					errno = 0;
					struct dirent* de = readdir(d);
					if (de == NULL)
						break;
					// check all characters
					char* c;
					int file_ok = 1;
					for (c = de->d_name; *c != 0; c = &c[1]) {
						if (!(('0' <= *c && *c <= '9')
								|| ('A' <= *c && *c <= 'Z')
								|| ('a' <= *c && *c <= 'z')
								|| (*c == '_' || *c == '-'))) {
							file_ok = 0;
							break;
						}
					}
					if (file_ok == 0)
						continue;

					c = strdup(de->d_name);
					log_passert(c != NULL, "Failed to allocate memory");

					list = (char**)realloc(list, sizeof(char*) * (list_size + 1));
					log_passert(list != NULL, "Failed to allocate memory");

					list[list_size] = c;
					list_size++;
				}
				log_passert(errno == 0, "Error performing readdir");
				if (list_size > 0)
					qsort(list, list_size, sizeof(char*), cmpstringp);
				closedir(d);
				size_t i;
				for (i = 0; i < list_size; i++) {
					char* file = alloca(strlen(dir) + strlen(list[i]) + 1);
					strcpy(file, dir);
					strcat(file, list[i]);
					_config_read(file, ctx);
					free(list[i]);
				}
				free(list);
			} else {
				log_die("[%s:%u]  Include '%s' is not a file or directory", filename, lineno, newfile);
			}
		}

		if (newfile != line)
			free(newfile);

		ctx->config_include_loop--;
	} else if (!ctx->parse_item(filename, ctx->ptr, lineno, keyword, line, len)) {
		log_die("[%s:%u]  Unknown config option '%s'", filename, lineno, keyword);
	}
}

static void
_config_read(const char* filename, struct config_read_context* ctx)
{
	int err;

	err = read_file(filename, ctx, (parse_line_t*)config_parse_line);
	if (err < 0) {
		if (err == -1)
			log_perror(CRIT, "[%s]  Failed reading from file", filename);
		else if (err == -2)
			log_msg(CRIT, "[%s]  Unexpected behaviour from read()", filename);
		else if (err == -3)
			log_msg(CRIT, "[%s]  File does not end with a newline character", filename);
		else if (err == -4)
			log_msg(CRIT, "[%s]  Line too long", filename);
		else if (err == -5)
			log_msg(CRIT, "[%s]  Decompression error", filename);
		else
			log_msg(CRIT, "[%s]  Unspecified error", filename);

		exit(1);
	}
}

void config_read(const char* filename, void* ptr, parse_item_t* parse_item)
{
	struct config_read_context ctx = { 0, ptr, parse_item };
	_config_read(filename, &ctx);
}
