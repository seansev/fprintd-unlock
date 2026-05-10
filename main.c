/*
 * fprintd-unlock - Spawn a screen locker; unlock it with a valid fingerprint.
 * Copyright (C) 2026 Sean S. Williams
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
*/

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"
#include "signum.h"

/*
 * Arguments
 */
static struct option longopts[] = {
        {"signal", required_argument, NULL, 's'},
        {"error-unlock", no_argument, NULL, 'e'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {0},
};

static const char usage[] =
        "\n"
        "Usage:\n  " PROGRAM_NAME " [options...] [--] [locker [args...]]\n"
        "\n"
        "Spawn a program, then send a signal when a valid fingerprint is read.\n"
        "\n"
        "Options:\n"
        "  -s, --signal <signal>\n"
        "    Set the signal to be passed when fingerprint verification"
                " succeeds.\n"
        "\n"
        "  -e, --error-unlock\n"
        "    Unlock in the event of an error. Considered insecure.\n"
        "\n"
        "  -h, --help\n"
        "    Display this help message.\n"
        "\n"
        "  -v, --version\n"
        "    Display the program version.\n"
        "\n"
        "For more details, see " PROGRAM_NAME "(1).\n"
        "\n";

/*
 * Globals
 */
static char *default_argv[] = {
        DEFAULT_LOCKER,
        NULL,
};

static int send_sig;
static bool error_unlock;
static pid_t locker_pid;

static int exit_code;
static bool locker_alive;

static int sig_pipe[2] = {-1, -1};
static volatile sig_atomic_t got_sigchld = 0;

/*
 * Helpers
 */
static void display_help()
{
        pid_t pid;
        int status;
        int null_fd;

        pid = fork();
        if (pid == 0) {
                /* hide stderr */
                null_fd = open("/dev/null", O_WRONLY);
                if (null_fd != -1)
                        dup2(null_fd, STDERR_FILENO);
                /* open manpage */
                execlp("man", "man", PROGRAM_NAME, NULL);
                exit(EXIT_FAILURE);
        }

        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS)
                fprintf(stderr, "%s", usage);

        exit(EXIT_SUCCESS);
}

static void display_version()
{
        printf("%s: v%s\n", PROGRAM_NAME, PROGRAM_VERSION);
        exit(EXIT_SUCCESS);
}

static void exit_unlock(int status)
{
        if (error_unlock)
                kill(locker_pid, send_sig);
        exit(status);
}

static void exit_if_errunlock(int status)
{
        if (error_unlock)
                exit(status);
}

static void drain_pipe(int pipe_fds[2])
{
        char buf[PIPE_BUF];

        while (read(pipe_fds[0], buf, sizeof(buf)) > 0);
}

/*
 * Callbacks
 */
static void on_sigchld(int sig)
{
        (void)sig;
        got_sigchld = 1;
        int errno_save = errno;
        write(sig_pipe[1], "1", 1);
        errno = errno_save;
}

/*
 * Stages
 */
static int register_signal_handlers(void)
{
        struct sigaction sa = {
                .sa_handler = on_sigchld,
                .sa_flags = SA_RESTART | SA_NOCLDSTOP,
        };
        sigemptyset(&sa.sa_mask);

        if (sigaction(SIGCHLD, &sa, NULL) < 0)
                return -1;

        return 0;
}

static void reap_locker(void)
{
        int status;
        pid_t pid;

        pid = waitpid(locker_pid, &status, WNOHANG);
        if (pid == locker_pid) {
                locker_alive = false;
                if (WIFEXITED(status))
                        exit_code = WEXITSTATUS(status);
                else if (WIFSIGNALED(status))
                        exit_code = 128 + WTERMSIG(status);
        } else if (pid == -1 && errno != ECHILD) {
                fprintf(stderr, "waitpid() error: %s\n", strerror(errno));
        }
}

/*
 * Main
 */
int main(int argc, char **argv)
{
        char *sigstr = DEFAULT_SIGNAL;
        char **locker_argv;

        error_unlock = false;

        /* getopt */
        int c;
        while ((c = getopt_long(argc, argv, "+s:ehv", longopts, NULL)) != -1) {
                switch (c) {
                case 's':
                        sigstr = optarg;
                        break;
                case 'e':
                        error_unlock = true;
                        break;
                case 'h':
                        display_help();
                        break;
                case 'v':
                        display_version();
                        break;
                case '?':
                        return EXIT_FAILURE;
                default:
                        return EXIT_FAILURE;
                }
        }

        /* get signal value */
        send_sig = parse_signal(sigstr);
        if (send_sig == 0) {
                fprintf(stderr, "Error: Invalid signal '%s' provided.\n", sigstr);
                exit_if_errunlock(EXIT_FAILURE);
                send_sig = parse_signal(DEFAULT_SIGNAL);
        }
        const char *abbrev = SIGABBREV(send_sig);
        if (abbrev)
                printf("Using signal %d. (SIG%s)\n", send_sig, abbrev);
        else
                printf("Using signal %d.\n", send_sig);

        /* self pipe */
        if (pipe2(sig_pipe, O_CLOEXEC | O_NONBLOCK) == -1) {
                fprintf(stderr, "pipe2() error: %s\n", strerror(errno));
                exit_if_errunlock(EXIT_FAILURE);
        }

        /* set up signals */
        if (register_signal_handlers() == -1) {
                perror("error registering signal handlers");
                exit_if_errunlock(EXIT_FAILURE);
        }

        /* get argv for locker */
        locker_argv = default_argv;
        if (optind < argc)
                locker_argv = &argv[optind];

        /* fork locker */
        locker_pid = fork();
        if (locker_pid < 0) {
                fprintf(stderr, "fork() error: %s\n", strerror(errno));
                return EXIT_FAILURE;
        }
        if (locker_pid == 0) {
                execvp(locker_argv[0], locker_argv);
                fprintf(stderr, "exec() error: %s\n", strerror(errno));
                _exit(127);
        }

        /* poll */
        exit_code = 0;
        locker_alive = true;

        while (locker_alive) {
                struct pollfd pfd = {
                        .fd = sig_pipe[0],
                        .events = POLLIN,
                };

                int n = poll(&pfd, 1, -1);
                if (n == -1) {
                        if (errno == EINTR)
                                continue;
                        fprintf(stderr, "poll() error: %s\n", strerror(errno));
                        exit_unlock(EXIT_FAILURE);
                }

                if (pfd.revents & POLLIN) {
                        drain_pipe(sig_pipe);
                }

                if (got_sigchld) {
                        got_sigchld = 0;
                        reap_locker();
                }
        }

        close(sig_pipe[0]);
        close(sig_pipe[1]);

        return exit_code;
}
