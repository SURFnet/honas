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

/*
 * Read a file line-by-line. Supports on the fly decompression of files.
 *
 * This is based on a cleaned-up version of 'read_file.c' from the qnet-dns-relayd.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#include "read_file.h"

#include "logging.h"

inline static int
read_file_close(int fd, pid_t pid)
{
	close(fd);
	if (pid != 0) {
		int status = -1;
		pid_t p = waitpid(pid, &status, WNOHANG);
		if (p == -1) {
			// FIXME: if we're running in a thread, the main
			//        loop can already have gotten the status. ignore these errors for now..
			if (errno == ECHILD)
				return 0;

			log_perror(ERR, "waitpid()");
			return -1;
		}
		int t_wait = 250; // 2500 ms
		while (p == 0 && t_wait > 0) {
			struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000 }; // 10 ms

			nanosleep(&ts, NULL);
			p = waitpid(pid, &status, WNOHANG);
			if (p == -1) {
				// FIXME: if we're running in a thread, the main
				//        loop can already have gotten the status. ignore these errors for now..
				if (errno == ECHILD)
					return 0;

				log_perror(ERR, "waitpid()");
				return -1;
			}
		}
		if (p == 0) {
			kill(pid, 9);
			status = -1;
			p = waitpid(pid, &status, 0);
			if (p == -1) {
				// FIXME: if we're running in a thread, the main
				//        loop can already have gotten the status. ignore these errors for now..
				if (errno == ECHILD)
					return 0;

				log_perror(ERR, "waitpid()");
				return -1;
			}
		}
		if (status != 0) {
			log_msg(ERR, "Status returned by child pid: %i", status);
			return -5;
		}
	}

	return 0;
}

int read_file(const char* filename, void* ptr, parse_line_t* parse_line)
{
	char* cmd = NULL;

	// support for compression
	int len = strlen(filename);
	if (len > 3 && strcmp(&filename[len - 3], ".gz") == 0) {
		cmd = "/bin/gunzip";
	} else if (len > 4 && strcmp(&filename[len - 4], ".bz2") == 0) {
		cmd = "/bin/bunzip2";
	} else if (len > 3 && strcmp(&filename[len - 3], ".xz") == 0) {
		cmd = "/usr/bin/unxz";
	}

	int fd = open(filename, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		return -1;

	pid_t pid = 0;
	if (cmd != NULL) {
		int pipefd[2];
		int err = pipe(pipefd);
		if (err == -1) {
			int _e = errno;
			close(fd);
			errno = _e;
			return -1; // errno error
		}

		// reroute fd through child process
		pid = fork();
		if (pid == -1) {
			int _e = errno;
			close(fd);
			errno = _e;
			return -1; // errno error
		}

		if (pid == 0) {
			/* child */
			int err = dup2(fd, 0); // stdin: file
			if (err == -1) {
				log_perror(ERR, "dup2()");
				_exit(1);
			}

			err = dup2(pipefd[1], 1); // stdout: pipe
			if (err == -1) {
				log_perror(ERR, "dup2()");
				_exit(1);
			}

			/* close all extra filedescriptors and reset the limit */
			rlim_t i;
			struct rlimit rlim;
			err = getrlimit(RLIMIT_NOFILE, &rlim);
			if (err == -1) {
				log_perror(ERR, "getrlimit(RLIMIT_NOFILE)");
				_exit(1);
			}
			for (i = 3; i < rlim.rlim_cur; i++)
				close(i);

			rlim.rlim_cur = sizeof(fd_set) * 8;
			rlim.rlim_max = sizeof(fd_set) * 8;
			err = setrlimit(RLIMIT_NOFILE, &rlim);
			if (err == -1) {
				log_perror(ERR, "setrlimit(RLIMIT_NOFILE)");
				_exit(1);
			}

			char* const argv[] = { cmd, NULL };
			char* const envp[] = { NULL };

			execve(cmd, argv, envp);
			log_perror(ERR, "execve()");
			_exit(1);
		}
		close(pipefd[1]);
		close(fd);

		fd = pipefd[0];
	}

	char buf[MAX_FILE_LINE_LENGTH];
	int buflen = 0;
	int lineno = 0;
	while (buflen < MAX_FILE_LINE_LENGTH) {
		int len;

		len = read(fd, &buf[buflen], MAX_FILE_LINE_LENGTH - buflen);
		if (len == -1) {
			int _e = errno;
			read_file_close(fd, pid);
			errno = _e;
			return -1; // errno error
		}

		if (len < 0) {
			read_file_close(fd, pid);
			return -2; // read() returned invalid value
		}

		buflen += len;

		if (buflen == 0) { // eof
			int err = read_file_close(fd, pid);
			return err;
		}

		char* idx = memchr(buf, '\n', buflen);
		if ((idx == NULL) && (len == 0)) {
			read_file_close(fd, pid);
			return -3; // file didn't end with a newline
		}

		if (idx != NULL) {
			*idx = 0;
			int i = (int)(idx - buf);
			lineno++;

			// call handler
			parse_line(filename, ptr, lineno, buf, i++);

			// look for more full lines
			while (1) {
				char* idx = memchr(&buf[i], '\n', buflen - i);
				if (idx == NULL)
					break;

				*idx = 0;
				int j = (int)(idx - &buf[i]);
				lineno++;

				// call handler
				parse_line(filename, ptr, lineno, &buf[i], j++);

				i += j;
			}

			// move to next line
			memmove(buf, &buf[i], buflen - i);
			buflen -= i;
		}
	}

	read_file_close(fd, pid);
	return -4; // line too long
}
