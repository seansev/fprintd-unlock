#include <errno.h>
#include <string.h>

#include "util.h"

void print_error(char *ctx, int err)
{
        if (err)
                fprintf(stderr, "%s error: %s\n", ctx, strerror(err));
        else
                fprintf(stderr, "%s error\n", ctx);
}
