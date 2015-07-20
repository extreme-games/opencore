/*
 * Copyright (c) 2007 cycad <cycad@zetasquad.com>
 *
 * This software is provided 'as-is', without any express or implied 
 * warranty. In no event will the author be held liable for any damages 
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute
 * it freely, subject to the following restrictions:
 *
 *     1. The origin of this software must not be misrepresented; you
 *     must not claim that you wrote the original software. If you use 
 *     this software in a product, an acknowledgment in the product 
 *     documentation would be appreciated but is not required.
 *
 *     2. Altered source versions must be plainly marked as such, and
 *     must not be misrepresented as being the original software.
 *
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 */

#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include <pthread.h>
#ifdef OSX
#include <sched.h>
#endif

#include <signal.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include "opencore.hpp"
#include "core.hpp"

#define OM_REPLY 0
#define OM_PUB 1
#define OM_ALERT 2

static
void
output_line(int output_mode, char *fmt, ...)
{
	char buf[300];

	va_list ap;
	va_start(ap, fmt);

	vsnprintf(buf, sizeof(buf)-1, fmt, ap);
	buf[sizeof(buf)-1] = '\0';

	if (output_mode == OM_PUB) {
		PubMessageFmt("Shell: %s", buf);
	} else if (output_mode == OM_ALERT) {
		PubMessageFmt("?alert %s", buf);
	} else if (output_mode == OM_REPLY) {
		ReplyFmt("Shell: %s", buf);
	}

	va_end(ap);
}


void
cmd_exec(CORE_DATA *cd)
{
	if (cd->cmd_argc < 2) {
		Reply("Usage: !exec <command> [arguments]");
		return;
	}

	/* create the pipes */
	int cstdin[2];
	int cstdout[2];

	if (pipe(cstdin) != 0) {
		Reply("Error creating stdin pipe");
		return;
	}
	if (pipe(cstdout) != 0) {
		Reply("Error creating stdout pipe");
		close(cstdin[0]);
		close(cstdin[1]);
		return;
	}

	int output_mode = OM_REPLY;
	char *cmd = cd->cmd_argr[1];
	if (strcmp(cd->cmd_argv[1], "-o") == 0 && cd->cmd_argc > 2) {
		output_mode = OM_PUB;
		cmd = cd->cmd_argr[2];
	} else if (strcmp(cd->cmd_argv[1], "-a") == 0 && cd->cmd_argc > 2) {
		output_mode = OM_ALERT;
		cmd = cd->cmd_argr[2];
	}

	pid_t pid = fork();
	if (pid == -1) {
		output_line(output_mode, "Error forking");
		return;
	} else if (pid == 0) {
		/* close unused portions of pipes */
		close(cstdin[1]);
		close(cstdout[0]);

		/* child */
		if (dup2(cstdin[0], STDIN_FILENO) == -1) {
			close(cstdin[0]);
			close(cstdout[1]);
			_exit(-1);
		}
		if (dup2(cstdout[1], STDOUT_FILENO) == -1) {
			close(cstdin[0]);
			close(cstdout[1]);
			_exit(-1);
		}

		/* close stdin even though it was just set up (dont want to hang on input) */
		close(STDIN_FILENO);

		/* redirect stderr */
		dup2(cstdout[1], STDERR_FILENO);

		char *shell = getenv("SHELL");
		if (shell == NULL) {
			shell = "/bin/sh";
		}

		char host[32];
		memset(&host, 0, sizeof(host));
		gethostname(host, 32);
		host[31] = '\0';

		execl(shell, shell, "-c", cmd, NULL);

#if 0
		this is commented out due to having no way to get the temporary files name
		to execute on the command line from the file descriptor (such as /bin/sh /tmp/tmpfile)

		this would be a better solution since it is shell agnostic. (no dependence on -c switch)

		FILE *f = tmpfile();
		if (f == NULL) {
			printf("Error creating temporary file\n");
			close(cstdin[0]);
			close(cstdout[1]);
			_exit(-1);
		}

		fprintf(f, "#!%s\n%s\n", shell, cd->cmd_argr[1 + o]);
		int fno = fileno(f);
		if (fno == -1) {
			printf("Error getting descriptor for temporary file\n");
			close(cstdin[0]);
			close(cstdout[1]);
			fclose(f);
			_exit(-1);
		}
#endif

		/* this is only reached in error */
		printf("Error in fexecve()\n");
		_exit(-1);
	} else {
		/* parent */
		close(cstdin[0]);
		close(cstdout[1]);

		/* wait for the child to exit max 5 seconds */
		int status;

		struct timeval t1, t2;
		gettimeofday(&t1, NULL); 
		t1.tv_sec += 3;	/* give it a few seconds to run */
		while (waitpid(pid, &status, WNOHANG) == 0) {
			gettimeofday(&t2, NULL);
			if (t2.tv_sec > t1.tv_sec) {
				kill(pid, SIGKILL);
				break;
			}
#ifdef OSX
			sched_yield();
#else
			pthread_yield();
#endif
		}

		char buf[240];
		memset(buf, 0, sizeof(buf));
		int i = 0;
		int r = 0;
		int lines = 0;

		struct utsname uts;
		bzero(&uts, sizeof(struct utsname));
		uname(&uts);

		output_line(output_mode, "%s@%s:%s > %s", cd->bot_name, uts.nodename, getenv("PWD"), cmd);

		do {
			r = read(cstdout[0], &buf[i], 1);

			if (i == (sizeof(buf) - 2)) {
				++i;
				buf[i] = '\n';
			}

			if (buf[i] == '\n') {
				buf[i] = '\0';
				output_line(output_mode, "%s", buf);
				i = 0;
				++lines;
			} else {
				++i;
			}
		} while (r == 1 && lines < 100);

		if (lines == 100) {
			output_line(output_mode, "%s", "[Output truncated at 100 lines]");
		} else {
			output_line(output_mode, "%s@%s:%s > ", cd->bot_name, uts.nodename, getenv("PWD"));
		}

		close(cstdout[0]);
		close(cstdin[1]);
	}
}

