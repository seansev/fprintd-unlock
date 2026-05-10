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
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
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

static int sig = -1;
static bool error_unlock = false;
static pid_t locker_pid = -1;

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
        if (error_unlock && locker_pid != -1 && sig != -1)
                kill(locker_pid, sig);
        exit(status);
}

static void exit_if_errunlock(int status)
{
        if (error_unlock)
                exit(status);
}

/*
 * Main
 */
int main(int argc, char **argv)
{
        char *sigstr = DEFAULT_SIGNAL;
        char **locker_argv;

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
        sig = parse_signal(sigstr);
        if (sig == 0) {
                fprintf(stderr, "Error: Invalid signal '%s' provided.\n", sigstr);
                exit_if_errunlock(EXIT_FAILURE);
                sig = parse_signal(DEFAULT_SIGNAL);
        }
        printf("Using signal %d. (SIG%s)\n", sig, sigabbrev_np(sig));

        /* get argv for locker */
        locker_argv = default_argv;
        if (optind < argc)
                locker_argv = &argv[optind];

        /* block signals */
        sigset_t sigset;

        sigemptyset(&sigset);
        sigaddset(&sigset, SIGCHLD);
        
        if (sigprocmask(SIG_BLOCK, &sigset, NULL) == -1) {
                fprintf(stderr, "sigprocmask() error: %s\n", strerror(errno));
                exit_if_errunlock(EXIT_FAILURE);
        }

        /* fork locker */
        locker_pid = fork();
        if (locker_pid < 0) {
                fprintf(stderr, "fork() error: %s\n", strerror(errno));
                return EXIT_FAILURE;
        }
        if (locker_pid == 0) {
                sigprocmask(SIG_UNBLOCK, &sigset, NULL);
                execvp(locker_argv[0], locker_argv);
                fprintf(stderr, "exec() error: %s\n", strerror(errno));
                _exit(127);
        }

        /* register signalfd */
        int sfd = signalfd(-1, &sigset, SFD_CLOEXEC);
        if (sfd == -1) {
                fprintf(stderr, "signalfd() error: %s\n", strerror(errno));
                exit_unlock(EXIT_FAILURE);
        }

        // TODO: epoll

        /* non-multiplexed read() approach */
        ssize_t size;
        struct signalfd_siginfo fdsi;
        int status = 0;

        for (;;) {
                size = read(sfd, &fdsi, sizeof(fdsi));
                if (size != sizeof(fdsi)) {
                        if (size == -1)
                                fprintf(stderr, "read() error: %s\n", strerror(errno));
                        else
                                fprintf(stderr, "read() error\n");
                        exit_unlock(EXIT_FAILURE);
                }

                if (fdsi.ssi_signo == SIGCHLD) {
                        pid_t wpid = waitpid(locker_pid, &status, WNOHANG);
                        if (wpid > 0)
                                break;
                }
        }

        if (WIFEXITED(status))
                printf("Code: %d\n", WEXITSTATUS(status));

        if (WIFSIGNALED(status))
                printf("Signal: %d\n", WTERMSIG(status));

        close(sfd);

        return EXIT_SUCCESS;
}
