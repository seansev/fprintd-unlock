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

#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"
#include "signum.h"

static struct option longopts[] = {
        {"signal", required_argument, NULL, 's'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {0},
};

const char usage[] =
        "\n"
        "Usage:\n  " PROGRAM_NAME " [options...] [--] [locker [args...]]\n"
        "\n"
        "Spawn a program, then send a signal when a valid fingerprint is read.\n"
        "\n"
        "Options:\n"
        "  -s, --signal <signal>\n"
        "    Set the signal to be passed when fingerprint verification succeeds.\n\n"
        "  -h, --help\n"
        "    Display this help message.\n\n"
        "  -v, --version\n"
        "    Display the program version.\n\n"
        "For more details, see " PROGRAM_NAME "(1).\n"
        "\n";

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

        if (!WIFEXITED(status))
                exit(EXIT_FAILURE);

        if (WEXITSTATUS(status) != EXIT_SUCCESS)
                fprintf(stderr, "%s", usage);

        exit(EXIT_SUCCESS);
}

static void display_version()
{
        printf("%s: v%s\n", PROGRAM_NAME, PROGRAM_VERSION);
        exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
        char *sigstr = DEFAULT_SIGNAL;
        int sig;

        int c;
        while ((c = getopt_long(argc, argv, "+s:hv", longopts, NULL)) != -1) {
                switch (c) {
                case 's':
                        sigstr = optarg;
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

        sig = parse_signal(sigstr);

        printf("%s -%d\n", sigstr, sig);

        return EXIT_SUCCESS;
}
