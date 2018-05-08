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

#ifndef LOGGING_H
#define LOGGING_H

#include "includes.h"

/** Basic logging api
 *
 * Features:
 * - Supports logging to stderr (default), file or syslog
 * - Using enum log level (for basic compile time checking)
 * - perror() like error message formatting (in the form of log_perror())
 * - assert_perror() like system error assertions (in the form of log_passert())
 * - Early exit on error (in the form of log_die() and log_pfail())
 *
 * Notes on usage:
 * - The logging may only be initialised and destroyed at most once per program invocation!
 * - Before initialisation and after destruction all messages will be logged to 'stderr'.
 * - It's fine to never call an initialisation function or to call destroy without calling an initialisation function.
 * - It's fine to change the minimum log level using before calling an initialisation function.
 * - When using the syslog target one should never call openlog() or closelog() themselves.
 * - When using the syslog target the minimum log level is also enforced for all "direct" syslog() invocations.
 *
 * \defgroup logging Basic logging facility
 */

/** Logging levels based on `syslog()` log levels */
typedef enum {
	UNSET = -1,
	EMERG = LOG_EMERG,
	EMERGENCY = LOG_EMERG,
	ALERT = LOG_ALERT,
	CRIT = LOG_CRIT,
	CRITICAL = LOG_CRIT,
	ERR = LOG_ERR,
	ERROR = LOG_ERR,
	WARN = LOG_WARNING,
	WARNING = LOG_WARNING,
	NOTICE = LOG_NOTICE,
	INFO = LOG_INFO,
	DEBUG = LOG_DEBUG
} log_level_t;

/** The log facility used when logging through syslog */
static const int DEFAULT_LOG_FACILITY = LOG_DAEMON;

/** Get the current minimum log level
 *
 * \note This function can be called before initialising the logging.
 *
 * \returns The current minimum log level
 * \ingroup logging
 */
extern log_level_t log_get_min_log_level(void);

/** Set the current minimum log level
 *
 * \note This function can be called before initialising the logging.
 *
 * \param min_log_level The new minimum log level
 * \ingroup logging
 */
extern void log_set_min_log_level(log_level_t min_log_level);

/** Initialize logging to file
 *
 * \param ident    Program identification string
 * \param filename File to write logging to
 * \ingroup logging
 */
extern void log_init_file(const char* ident, const char* filename);

/** Initialize logging to syslog
 *
 * \param ident    Program identification string
 * \param facility Syslog facility to use (if 0 it uses DEFAULT_LOG_FACILITY)
 * \ingroup logging
 */
extern void log_init_syslog(const char* ident, int facility);


/** Re-open the logging output
 *
 * \note When applicable for the log destination, it's always
 *       safe to call this function, but it might not have
 *       any effects.
 *
 * \ingroup logging
 */
extern void log_reopen(void);


/** Log a formatted message with the arguments in a `va_list`
 *
 * \param log_level The log level for this message
 * \param format    The 'printf' format string for this message
 * \param ap        The list of variables needed for the given format string
 * \ingroup logging
 */
extern void log_msg_va_list(log_level_t log_level, const char* format, va_list ap);

/** Log a formatted message
 *
 * \param log_level The log level for this message
 * \param format    The 'printf' format string for this message
 * \param ...       Any additional variables needed for this message
 * \ingroup logging
 */
extern void log_msg(log_level_t log_level, const char* format, ...);

/** Exit with a critical error message being logged
 *
 * \param format The 'printf' format string for the log message
 * \param ...    Any additional variables needed for the log message
 * \ingroup logging
 */
#define log_die(...)                \
	do {                            \
		log_msg(CRIT, __VA_ARGS__); \
		exit(1);                    \
	} while (0)


/** Log a formatted message with the arguments in a `va_list` including the system error message
 *
 * \param log_level The log level for this message
 * \param format    The 'printf' format string for this message
 * \param ap        The list of variables needed for the given format string
 * \ingroup logging
 */
extern void log_perror_va_list(log_level_t log_level, const char* format, va_list ap);

/** Log a formatted message including the system error message
 *
 * \param log_level The log level for this message
 * \param format    The 'printf' format string for this message
 * \param ...       Any additional variables needed for this message
 * \ingroup logging
 */
extern void log_perror(log_level_t log_level, const char* format, ...);


/** Exit with a critical error message being logged including the system error message
 *
 * \param format The 'printf' format string for the log message
 * \param ...    Any additional variables needed for the log message
 * \ingroup logging
 */
#define log_pfail(...)                 \
	do {                               \
		log_perror(CRIT, __VA_ARGS__); \
		exit(1);                       \
	} while (0)

/** If the given condition is false then exit with a critical error message being logged including the system error message
 *
 * \param cond   A condition that, when it is false, will trigger an `log_pfail` with the given message
 * \param format The 'printf' format string for the log message
 * \param ...    Any additional variables needed for the log message
 * \ingroup logging
 */
#define log_passert(cond, ...)      \
	do {                            \
		if (!(cond))                \
			log_pfail(__VA_ARGS__); \
	} while (0)


/** Logging destroy
 *
 * It's recommended to call this function at the end of the program lifetime.
 * Depending on the output type being used it might flush output buffers
 * and release possible resources.
 *
 * \ingroup logging
 */
extern void log_destroy(void);

#endif /* LOGGING_H */
