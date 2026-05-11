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
#include "dbus.h"
#include "signum.h"
#include "util.h"

/*
 * Arguments
 */
static struct option longopts[] = {
        {"signal", required_argument, NULL, 's'},
        {"finger", required_argument, NULL, 'f'},
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
        "  -f, --finger <finger>\n"
        "    Finger selected to verify.\n"
        "\n"
        "  -e, --error-unlock\n"
        "    Unlock in the event of an error. Considered insecure.\n"
        "\n"
        "  -h, --help\n"
        "    Display this help message.\n"
        "\n"
        "  -v, --version\n"
        "    Display the program version.\n"
#ifdef MAN_PAGES
        "\n"
        "For more details, see " PROGRAM_NAME "(1).\n"
#endif
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
static volatile sig_atomic_t got_sigint = 0;
static volatile sig_atomic_t got_sigterm = 0;

static sd_bus *bus;
static char *device_path;

/*
 * Helpers
 */
static void display_help(void)
{
#ifdef MAN_PAGES
        pid_t pid;
        int status;
        int null_fd;

        pid = fork();
        if (pid == 0) {
                /* hide stderr, errors are OK just ignore */
                null_fd = open("/dev/null", O_WRONLY);
                if (null_fd != -1)
                        dup2(null_fd, STDERR_FILENO);
                /* open manpage */
                execlp("man", "man", PROGRAM_NAME, NULL);
                exit(EXIT_FAILURE);
        }

        waitpid(pid, &status, 0);

        /* don't show static help if man succeeded */
        if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS)
                exit(EXIT_SUCCESS);
#endif

        fprintf(stderr, "%s", usage);

        exit(EXIT_SUCCESS);
}

static void display_version(void)
{
        printf("%s: v%s\n", PROGRAM_NAME, PROGRAM_VERSION);
        exit(EXIT_SUCCESS);
}

static void unlock_if_errunlock(void)
{
        if (error_unlock)
                kill(locker_pid, send_sig);
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
static void on_signal(int sig)
{
        switch (sig) {
        case SIGCHLD:
                got_sigchld = 1;
                break;
        case SIGINT:
                got_sigint = 1;
                break;
        case SIGTERM:
                got_sigterm = 1;
                break;
        }
        int errno_save = errno;
        (void)write(sig_pipe[1], "1", 1);
        errno = errno_save;
}

/*
 * Stages
 */
static int register_signal_handlers(void)
{
        struct sigaction sa = {
                .sa_handler = on_signal,
                .sa_flags = SA_RESTART | SA_NOCLDSTOP,
        };
        sigemptyset(&sa.sa_mask);

        if (    sigaction(SIGCHLD, &sa, NULL) < 0
             || sigaction(SIGINT, &sa, NULL) < 0
             || sigaction(SIGTERM, &sa, NULL) < 0)
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

static int init_bus(void)
{
        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_message *reply = NULL;
        const char *path;
        int ret;

        ret = sd_bus_default_system(&bus);
        if (ret < 0) {
                fprintf(stderr, "Failed to connect to D-Bus system bus. Is the system bus installed and running?\n");
                print_error("sd_bus_default_system()", -ret);
                goto err;
        }

        ret = sd_bus_call_method(bus, FPRINT_BUS, MGR_PATH, MGR_IFACE,
                        "GetDefaultDevice", &error, &reply, "");
        if (ret < 0) {
                fprintf(stderr, "Failed to get default fprint device. Is fprintd running?\n");
                goto err;
        }

        ret = sd_bus_message_read(reply, "o", &path);
        if (ret < 0) {
                print_error("GetDefaultDevice reply", -ret);
                goto err;
        }

        device_path = strdup(path);
        if (device_path == NULL) {
                print_error("Device path strdup()", errno);
                goto err;
        }

        sd_bus_message_unref(reply);
        sd_bus_error_free(&error);
        return 0;

err:
        sd_bus_message_unref(reply);
        sd_bus_error_free(&error);
        return -1;
}

static void close_bus(void)
{
        free(device_path);
        device_path = NULL;
        sd_bus_unref(bus);
        bus = NULL;
}

static int claim_device(void)
{
        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_message *reply = NULL;
        int ret;
        int attempts;

        for (attempts = 0; attempts < 10; attempts++) {
                ret = sd_bus_call_method(bus, FPRINT_BUS, device_path,
                                DEV_IFACE, "Claim", &error, &reply, "s", "");
                if (ret >= 0) {
                        sd_bus_message_unref(reply);
                        sd_bus_error_free(&error);
                        return 0;
                }

                if (error.name && strcmp(error.name,
                                FPRINT_BUS ".Error.AlreadyInUse") == 0) {
                        fprintf(stderr, "Claim error: Fingerprint device in use.\n");
                        sd_bus_message_unref(reply);
                        reply = NULL;
                        sd_bus_error_free(&error);
                        error = SD_BUS_ERROR_NULL;
                        sleep(1);
                        continue;
                }

                if (error.message)
                        fprintf(stderr, "Claim failed: %s (%s)\n",
                                        error.message, error.name);
                else
                        print_error("Claim", -ret);
                
                goto err;
        }

        fprintf(stderr, "Claim failed after %d attempts.\n", attempts);

err:
        sd_bus_message_unref(reply);
        sd_bus_error_free(&error);
        return -1;
}

static int release_device(void)
{
        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_message *reply = NULL;
        int ret;

        ret = sd_bus_call_method(bus, FPRINT_BUS, device_path, DEV_IFACE,
                        "Release", &error, &reply, "");
        if (ret < 0) {
                if (error.message)
                        fprintf(stderr, "Release failed: %s (%s)\n",
                                        error.message, error.name);
                else
                        print_error("Release", -ret);
                ret = -1;
        } else
                ret = 0;

        sd_bus_message_unref(reply);
        sd_bus_error_free(&error);
        return ret;
}

/*
 * Main
 */
int main(int argc, char **argv)
{
        char *sigstr = DEFAULT_SIGNAL;
        char *finger = "any";
        char **locker_argv;

        error_unlock = false;

        /* getopt */
        int c;
        while ((c = getopt_long(argc, argv, "+s:f:ehv", longopts, NULL)) != -1) {
                switch (c) {
                case 's':
                        sigstr = optarg;
                        break;
                case 'f':
                        finger = optarg;
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

        printf("Finger '%s' selected\n", finger);

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
                print_error("pipe2()", errno);
                exit_if_errunlock(EXIT_FAILURE);
        }

        /* set up signals */
        if (register_signal_handlers() == -1) {
                print_error("Signal handler registration", 0);
                exit_if_errunlock(EXIT_FAILURE);
        }

        /* get argv for locker */
        locker_argv = default_argv;
        if (optind < argc)
                locker_argv = &argv[optind];

        /* fork locker */
        locker_pid = fork();
        if (locker_pid < 0) {
                print_error("fork()", errno);
                return EXIT_FAILURE;
        }
        if (locker_pid == 0) {
                execvp(locker_argv[0], locker_argv);
                fprintf(stderr, "Failed to execute '%s'. Is the path correct?\n", locker_argv[0]);
                print_error("exec()", errno);
                _exit(127);
        }

        /* initialize dbus */
        if (init_bus() < 0) {
                unlock_if_errunlock();
                exit_code = EXIT_FAILURE;
                goto out_pipe;
        }

        printf("Fprint device available at %s\n", device_path);

        if (claim_device() < 0) {
                unlock_if_errunlock();
                exit_code = EXIT_FAILURE;
                goto out_bus;
        }

        printf("Device claimed: %s\n", device_path);

        /* poll */
        exit_code = 0;
        locker_alive = true;

        while (locker_alive) {
                enum {PFD_SIGNAL, PFD_BUS, PFD_COUNT};
                struct pollfd pfds[PFD_COUNT] = {
                        [PFD_SIGNAL] = {
                                .fd = sig_pipe[0],
                                .events = POLLIN,
                        },
                };

                int n = poll(pfds, PFD_COUNT, -1);
                if (n == -1) {
                        if (errno == EINTR)
                                continue;
                        print_error("poll()", errno);
                        unlock_if_errunlock();
                        exit_code = EXIT_FAILURE;
                        goto out_release;
                }

                if (pfds[PFD_SIGNAL].revents & POLLIN) {
                        drain_pipe(sig_pipe);
                }

                if (got_sigchld) {
                        got_sigchld = 0;
                        reap_locker();
                }

                if (got_sigint || got_sigterm)
                        break;
        }

out_release:
        release_device();
        printf("Device released: %s\n", device_path);

out_bus:
        close_bus();

out_pipe:
        close(sig_pipe[0]);
        close(sig_pipe[1]);

        return exit_code;
}
