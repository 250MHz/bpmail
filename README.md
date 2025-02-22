# bpmail

bpmail provides a set of utilities for sending and receiving email over Bundle
Protocol (BP) using Interplanetary Overlay Network (ION).

bpmail implements the method for delivering SMTP messages over BP described in
Scott Johnson's Internet Draft [I-D.johnson-dtn-interplanetary-smtp](https://datatracker.ietf.org/doc/draft-johnson-dtn-interplanetary-smtp/00/).

## Requirements

* ION 4.1.3s (built with Mbed TLS)
* Meson
* (Optional) pytest

## Installation

(Optional) To run the regression tests, Meson must be able to find a Python
installation with pytest. You could install pytest in a [virtual environment](https://docs.python.org/3/library/venv.html).
Activate the virtual environment and install pytest before continuing with the
build instructions.

```
meson setup build
meson compile -C build
```

If `meson setup` could find pytest, then you can run the tests with:

```
meson test -C build
```

