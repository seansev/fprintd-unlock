#pragma once

/* Pattern borrowed from mako */
#if defined(HAVE_LIBSYSTEMD)
#include <systemd/sd-bus.h>
#elif defined(HAVE_LIBELOGIND)
#include <elogind/sd-bus.h>
#elif defined(HAVE_BASU)
#include <basu/sd-bus.h>
#endif

#define FPRINT_BUS    "net.reactivated.Fprint"
#define MGR_PATH      "/net/reactivated/Fprint/Manager"
#define MGR_IFACE     "net.reactivated.Fprint.Manager"
#define DEV_IFACE     "net.reactivated.Fprint.Device"
