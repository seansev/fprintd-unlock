#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "signum.h"

/* Converted from NetBSD's signalnumber(3) */
int signalnumber(const char *name)
{
        int i;

        if (strncasecmp(name, "sig", 3) == 0)
                name += 3;

        for (i = 1; i < NSIG; ++i)
                if (sigabbrev_np(i) != NULL &&
                        strcasecmp(name, sigabbrev_np(i)) == 0)
                        return i;

        return 0;
}

int parse_signal(const char *str)
{
        int sig;

        sig = atoi(str);
        if (sig > 0 && sig < NSIG)
                return sig;

        sig = signalnumber(str);
        return sig;
}
