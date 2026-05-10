# fprintd-unlock

This program is a wrapper for Linux screen lockers (namely `swaylock`) which enables simple and secure unlocking using a fingerprint reader.

## Dependencies

- `fprintd`: D-Bus daemon for interfacing with fingerprint hardware.
- `libsystemd` | `libelogind` | `basu`: D-Bus client via `sd-bus`. Any of these three libraries will suffice, and can optionally be chosen between with the `sd-bus-provider` build option.
- `meson` & `ninja`: Build system.
- `scdoc` (Optional): For generating manpages.

## Installation

1. Clone the repo.
2. Configure: `meson setup build`
3. Build: `ninja -C build`
4. Install: `sudo ninja -C build install`

To uninstall: `sudo ninja -C build uninstall`

An XBPS template for Void Linux will be provided at a later date.

## Usage

See man **fprintd-unlock**(1) or `fprintd-unlock --help` after installation.

## Footnotes

Written by Sean S. Williams. Reach me at mail@seansev.com.

> This project is published under the MIT license. You may do as you please with this code, as long as the original copyright notice is included. This software is provided without warranty.
