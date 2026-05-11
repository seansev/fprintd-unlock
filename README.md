# fprintd-unlock

This program is a wrapper for graphical screen lockers (namely `swaylock`) which enables simple and secure unlocking using a fingerprint reader.

## Dependencies

**Build-time:**
- `meson` & `ninja`: Build system.
- `libsystemd` | `libelogind` | `basu`: D-Bus client via `sd-bus`. If you're on a Linux distro with systemd (Ubuntu, Fedora, Arch, etc.) you already have this. Otherwise, you'll have to install `libelogind` or `basu` (e.g. on Void, Artix, FreeBSD). This library is auto-detected, but you can choose one with the `sd-bus-provider` build option.
- `scdoc` (Optional): For generating manpages.

**Runtime:**
- `fprintd`: D-Bus daemon for interfacing with fingerprint hardware.
- `dbus`: D-Bus. Installed on every Linux distro, but also needed for FreeBSD.

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
