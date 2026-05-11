#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "signum.h"
#include "util.h"

static const char *usage = "Usage: %s <signal>\n";

static volatile sig_atomic_t signaled = 0;
static int expected_sig;

static void on_signal(int sig)
{
        if (sig == expected_sig)
                signaled = 1;
}

int main(int argc, char **argv)
{
        if (argc >= 2)
                if (strcmp(argv[1], "-h") == 0
                 || strcmp(argv[1], "--help") == 0
                 || strcmp(argv[1], "help") == 0) {
                        printf(usage, argv[0]);
                        /*
                         * Don't exit with 0 because that indicates unlock.
                         * Don't exit with 1 because that indicates error.
                         */
                        exit(3);
                } else
                        expected_sig = parse_signal(argv[1]);
        else
                expected_sig = parse_signal(DEFAULT_SIGNAL);

        if (expected_sig == 0)
                expected_sig = parse_signal(DEFAULT_SIGNAL);

        struct sigaction sa = {
                .sa_handler = on_signal,
        };
        sigemptyset(&sa.sa_mask);
        if (sigaction(expected_sig, &sa, NULL) < 0) {
                print_error("sigaction()", errno);
                exit(EXIT_FAILURE);
        }

        printf("PID: %d\n", getpid());

        while (!signaled)
                pause();

        return EXIT_SUCCESS;
}
