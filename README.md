# Advanced LEGO Printer System

A multifunctional system for controlling a homemade LEGO-based 2D printer with image and text processing and G-code generation for printing.

## Project overview

This is a comprehensive solution consisting of several interconnected modules:
- Printer driver - low-level control of LEGO HUB via Bluetooth
- G-code interpreter - checking, compiling, executing CNC commands
- Path Generator - convert images and text into vector paths
- Font system - support for custom and built-in fonts
- Handwriting emulator - intelligent character connection

## Current status

### Implemented

Printer Driver (LegoPrinterCore)
- Connecting to LEGO HUB via Bluetooth LE
- Motor Control (Speed, Direction, Rotation Angles)
- Logging and Error Handling
- Thread-Safe Operations
- Wait for Operation Completion Commands

G-code interpreter
- Parsing and execution of basic G-codes (G0, G1, G28, G90, G91)
- M-code support (M30 - stop)
- Stepper configuration via files
- Two-pass processing (check + execute)
- Progress and execution status system
- Detailed logging and error handling

Text Processing
- Edge Detection
- Filtering and Analysis of Geometric Properties
- Generating Test G-Code Sequences for Text Printing

### In active development

G-code generation for any image, text with any font, and underscores
- Convert vector contours to G-code
- Optimize motion paths

Font System
- Loading custom fonts from PNG
- Text rendering with Unicode support
- Cursive emulation
- Automatic character joining

User Interfaces
- Windows Forms Application
- Android Application

### Planned

Kernel Improvements
- Crash Stop and Pause
- Interruptible Operations
- Caching and Preview
- Batch Job Processing

Advanced Features
- Automatic Axis Calibration
- Software Backlash Compensation
- Multi-Color Printing Support
- Multi-Page Printing Support