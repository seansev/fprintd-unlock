#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "signum.h"

#if defined(HAVE_SYS_SIGNAME)
        #define SIGABBREV(i) (sys_signame[i])
#elif defined(HAVE_SIGABBREV_NP)
        #define SIGABBREV(i) (sigabbrev_np(i))
#else
        #define SIGABBREV(i) (NULL)
#endif

/* Converted from NetBSD's signalnumber(3) */
int signalnumber(const char *name)
{
        int i;

        if (strncasecmp(name, "sig", 3) == 0)
                name += 3;

        for (i = 1; i < NSIG; ++i) {
                const char *abbrev = SIGABBREV(i);
                if (abbrev != NULL &&
                        strcasecmp(name, abbrev) == 0)
                        return i;
        }

        return 0;
}

int parse_signal(const char *str)
{
        long sig;
        char *endptr;

        if (!str || !*str)
                return 0;

        errno = 0;
        sig = strtol(str, &endptr, 10);
        if (errno != 0 || *endptr != '\0' || sig <= 0 || sig >= NSIG)
                sig = (long)signalnumber(str);

        return (int)sig;
}
