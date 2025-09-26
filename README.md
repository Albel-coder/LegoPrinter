Printer Driver

A simple driver for controlling a homemade 2D printer.
Basic functions for working with the hub and motors have been implemented and tested to a state-of-the-art standard.

Features (implemented)

* Connect to the hub and check the connection status.

* Control the built-in LED indicator on the hub (on/off, simple tests).

* Rotate the motors at a specified angle (with direction and speed support).

Project Status

Version: v0.1 (tested & working)

The code has been tested on the target hub, and the basic functionality is stable.

Development Plans

- Implement emergency stop.
- Improve step generation (timers/interrupts instead of blocking calls).
- Integrate with the G-code interpreter and UI.
- Generation G-code for full printing.
