# Readsb

[Portmanteau of *Read ADSB*]

Readsb is a Mode-S/ADSB/TIS decoder for RTLSDR, BladeRF, Modes-Beast and GNS5894 devices.
As a former fork of [dump1090-fa](https://github.com/flightaware/dump1090) it is using that code base
but development will continue as a standalone project with new name. Readsb can co-exist on the same
host system with dump1090-fa, it doesn't use or modify its resources. However both programs will not
share a receiver device at the same time and in parallel.

This version uses Googles protocol buffer for data storage and exchange with web application.
Saves up to 70% in storage space and bandwidth.

## USRP Support
I added USRP support for device x300. See the INSTALL guide for details.

## readsb Debian/Raspbian/Ubuntu packages

See [INSTALL](INSTALL.md) for installation and build process.
