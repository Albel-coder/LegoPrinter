package com.example.lpstudio.jni;

// Class for loading native libraries
// Separate library loading and controller logic
public class PrinterJNI {
    static {
        // Loading libraries in the correct order
        System.loadLibrary("printer_jni");
    }

    // This class has no methods, only a static initialization block.
    // All native methods are declared in PrinterController
}
