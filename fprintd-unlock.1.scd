fprintd-unlock(1)

# NAME

fprintd-unlock - Fingerprint screen unlocking via fprint

# SYNOPSIS

*fprintd-unlock* [*-*_signal_|*-s* _signal_] [*--* _program_ [_args..._]]

Spawns a program, then sends a signal when a valid fingerprint is read.

# OPTIONS
*-s, --signal* <signal>
	The signal to be sent to the child process when a valid fingerprint is detected. Defaults to *SIGUSR1*.

*-*<signum>
	Same as *-s*, but takes the signal number as the argument.

*--* <executable> <args>
	Command to execute once fprint is ready, separated from other args by the *--* delimiter. Defaults to *swaylock*.

# SIGNALS

*SIGUSR1*
	Sends the configured unlock signal to the child process.

# AUTHORS

Written by Sean S. Williams <mail@seansev.com>.
