#pragma once

#if defined(HAVE_SYS_SIGNAME)
        #define SIGABBREV(i) (sys_signame[i])
#elif defined(HAVE_SIGABBREV_NP)
        #define SIGABBREV(i) (sigabbrev_np(i))
#else
        #define SIGABBREV(i) (NULL)
#endif

int signalnumber(const char *name);
int parse_signal(const char *str);
