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

#include "logging.h"

static const log_level_t DEFAULT_LOG_LEVEL = NOTICE;

struct {
	const char *name;
	log_level_t log_level;
} prioritynames[] = {
	{ "emerg", EMERG },
	{ "alert", ALERT },
	{ "crit", CRIT },
	{ "err", ERR },
	{ "warn", WARNING },
	{ "notice", NOTICE },
	{ "info", INFO },
	{ "debug", DEBUG },
	{ NULL, UNSET }
};

static struct {
	enum {
		target_uninitialized,
		target_destroyed,
		target_file,
		target_syslog
	} target;

	log_level_t min_log_level;

	union {
		struct {
			char* ident;
			char* filename;
			FILE* fh;
		} file;
	} config;
} ctx = { target_uninitialized, UNSET, { { 0 } } };

void log_init_file(const char* ident, const char* filename)
{
	assert(ctx.target == target_uninitialized);

	ctx.config.file.ident = strdup(ident);
	log_passert(ctx.config.file.ident != NULL, "Unable to initialize 'file' log, unable to clone ident");
	ctx.config.file.filename = strdup(filename);
	log_passert(ctx.config.file.filename != NULL, "Unable to initialize 'file' log, unable to clone filename");
	ctx.config.file.fh = fopen(filename, "a");
	log_passert(ctx.config.file.fh != NULL, "Failed to open log file '%s'", filename);

	ctx.target = target_file;
}

void log_init_syslog(const char* ident, int facility)
{
	assert(ctx.target == target_uninitialized);

	if (facility == 0)
		facility = DEFAULT_LOG_FACILITY;

	openlog(ident, LOG_NDELAY | LOG_PID, facility);
	setlogmask(LOG_UPTO(ctx.min_log_level));
	ctx.target = target_syslog;
}

extern void log_reopen(void)
{
	switch (ctx.target) {
	case target_file:
		ctx.target = target_uninitialized; /* in case 'log_passert' fails we want to log to stderr */
		log_passert(fclose(ctx.config.file.fh) == 0, "Failed to close logfile");
		ctx.config.file.fh = fopen(ctx.config.file.filename, "a");
		log_passert(ctx.config.file.fh != NULL, "Failed to open log file '%s'", ctx.config.file.filename);
		ctx.target = target_file;
		return;

	default:
		// Nothing to do
		break;
	}
}

log_level_t log_get_min_log_level(void)
{
	if (ctx.min_log_level != UNSET)
		return ctx.min_log_level;

	char* val = getenv("LOG_LEVEL");
	if (val == NULL) {
		ctx.min_log_level = DEFAULT_LOG_LEVEL;
		return ctx.min_log_level;
	}

	size_t i = 0;
	while (prioritynames[i].name != NULL) {
		if (strcmp(val, prioritynames[i].name) == 0) {
			ctx.min_log_level = prioritynames[i].log_level;
			return ctx.min_log_level;
		}
	}

	ctx.min_log_level = DEFAULT_LOG_LEVEL;
	log_msg(WARN, "Unsupported minimum log level '%s' specified in environment", val);
	return ctx.min_log_level;
}

void log_set_min_log_level(log_level_t min_log_level)
{
	ctx.min_log_level = min_log_level;
	switch (ctx.target) {
	case target_syslog:
		setlogmask(LOG_UPTO(min_log_level));
		return;

	default:
		// Nothing to do
		break;
	}
}

static const char *log_level_name(log_level_t log_level)
{
	size_t i = 0;
	while (prioritynames[i].name != NULL)
		if (prioritynames[i].log_level == log_level)
			return prioritynames[i].name;
	return "unknown";
}

static void log_print_file_line_prefix(log_level_t log_level)
{
	char timestamp[] = "YYYY-MM-DD HH:MM:SS";
	time_t now = time(NULL);
	struct tm now_tm;
	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime_r(&now, &now_tm));
	fprintf(ctx.config.file.fh, "%s %s[%d] %s: ", timestamp, ctx.config.file.ident, getpid(), log_level_name(log_level));
}

void log_msg_va_list(log_level_t log_level, const char* format, va_list ap)
{
	if (log_level > log_get_min_log_level())
		return;

	switch (ctx.target) {
	case target_uninitialized:
	case target_destroyed:
		vfprintf(stderr, format, ap);
		putc('\n', stderr);
		break;

	case target_file:
		log_print_file_line_prefix(log_level);
		vfprintf(ctx.config.file.fh, format, ap);
		putc('\n', ctx.config.file.fh);
		break;

	case target_syslog:
		vsyslog(log_level, format, ap);
		break;
	}
}

void log_msg(log_level_t log_level, const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	log_msg_va_list(log_level, format, ap);
	va_end(ap);
}

void log_perror_va_list(log_level_t log_level, const char* format, va_list ap)
{
	if (log_level > log_get_min_log_level())
		return;

	switch (ctx.target) {
	case target_uninitialized:
	case target_destroyed:
		vfprintf(stderr, format, ap);
		fprintf(stderr, ": %s\n", strerror(errno));
		break;

	case target_file:
		log_print_file_line_prefix(log_level);
		vfprintf(ctx.config.file.fh, format, ap);
		fprintf(ctx.config.file.fh, ": %s\n", strerror(errno));
		break;

	case target_syslog:
		{ /* Make sure to send all the information in one call to syslog() */
			size_t logbuf_size = vsnprintf(NULL, 0, format, ap) + 1;
			char *logbuf = alloca(logbuf_size);
			vsnprintf(logbuf, logbuf_size, format, ap);
			syslog(log_level, "%s: %s", logbuf, strerror(errno));
		}
		break;
	}
}

void log_perror(log_level_t log_level, const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	log_perror_va_list(log_level, format, ap);
	va_end(ap);
}

void log_destroy(void)
{
	switch (ctx.target) {
	case target_uninitialized:
	case target_destroyed:
		// nothing to do
		break;

	case target_file:
		ctx.target = target_uninitialized;
		log_passert(fclose(ctx.config.file.fh) == 0, "Failed to close logfile");
		ctx.config.file.fh = NULL;
		free(ctx.config.file.ident);
		ctx.config.file.ident = NULL;
		free(ctx.config.file.filename);
		ctx.config.file.filename = NULL;
		break;

	case target_syslog:
		closelog();
		break;
	}
	ctx.target = target_destroyed;
}
