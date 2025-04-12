# bpmail

bpmail provides a set of utilities for sending and receiving email over Bundle
Protocol (BP) using Interplanetary Overlay Network (ION).

bpmail implements the method for delivering SMTP messages over BP described in
Scott Johnson's Internet Draft [I-D.johnson-dtn-interplanetary-smtp](https://datatracker.ietf.org/doc/draft-johnson-dtn-interplanetary-smtp/00/).

## Requirements

[//]: # (TODO: list min version that passes our tests, currently just listing
the versions we developed with)

* ION 4.1.3s (built with Mbed TLS)
* GMime 3.2.15 (built with Libidn2)
* c-ares 1.34.4
* Meson
* (Optional) pytest, dnslib

## Installation

(Optional) To run the regression tests, Meson must be able to find a Python
installation with pytest and dnslib. You could install these packages in a
[virtual environment](https://docs.python.org/3/library/venv.html).
Activate the virtual environment and install the packages before continuing with
the build instructions.

```
meson setup build
meson compile -C build
```

If `meson setup` could find pytest, then you can run the tests with:

```
meson test -C build
```
