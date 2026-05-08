fprintd-unlock(1)

# NAME

fprintd-unlock - Fingerprint screen unlocking via fprint

# SYNOPSIS

*fprintd-unlock* [*-s* _signal_] [*--*] [_program_ [_args..._]]

Spawns a program, then sends a signal when a valid fingerprint is read.

# OPTIONS

*-s, --signal* <signal>
	The signal to be sent to the child process when a valid fingerprint is
	detected. It may be given as a name or a number. Defaults to *SIGUSR1*.

*--* <program> <args>
	The command to execute once fprint is ready. Separated from other args
	by the *--* delimiter, if needed. Defaults to *swaylock*.

# SIGNALS

*SIGUSR1*
	Sends the configured unlock signal to the child process.

# AUTHORS

Written by Sean S. Williams <mail@seansev.com>.

# SEE ALSO

*swaylock*(1) *fprintd*(1) *kill*(1)
