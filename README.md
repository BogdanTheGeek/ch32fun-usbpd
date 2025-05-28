USB Power Delivery Library for [ch32fun](https://github.com/cnlohr/ch32fun)

 > [!WARN]
 > This library has been written from the ground up only using the references listed below.
 > Things might not work as expected. If you encounter any issues, open a ticket or, even better, submit a PR.

## Features
 - [x] Automatic CC line detection
 - [x] Sink Support
 - [x] Programmable Power Supply (PPS) Sink Support
 - [ ] Spec Compliant Timeouts
 - [ ] Spec Compliant State Machine
 - [ ] Packet Monitoring
 - [ ] Source Support
 - [ ] Extended Messages (sec. 6.5 of the spec)

## Usage
1. Copy `usbpd.h` into your project.
1. Look at `main.c` to learn how to use it.

Alternatively, you can just get the repo(and init the submodule), run `make` and start hacking.

## References
 - [USB PD Spec](https://www.usb.org/document-library/usb-power-delivery)
 - [TI USB PD App note](https://www.ti.com/lit/an/slva842/slva842.pdf?ts=1747719230043)
 - [CH32X035 Reference Manual](https://www.wch-ic.com/downloads/CH32X035RM_PDF.html)
