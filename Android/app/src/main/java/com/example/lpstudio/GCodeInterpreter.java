package com.example.lpstudio;

import android.util.Printer;

import java.util.ArrayList;
import java.util.List;

// Java wrapper for C++ g-code interpreter
// C# GCodeInterpreter Analog
public class GCodeInterpreter implements AutoCloseable {

    // Pointer to the native interpreter object (stored as long)
    private long interpreterHandle = 0;
    private long printerHandle = 0;
    private boolean disposed = false;

    // Status enum
    public enum Status {
        IDLE(0),
        CHECKING_CODE(1),
        RUNNING(2),
        PAUSED(3),
        COMPLETED(4),
        ERROR(5);

        private final int value;

        Status(int value) {
            this.value = value;
        }

        public int getValue() {
            return value;
        }

        public static Status fromValue(int value) {
            for (Status status : values()) {
                if (status.value == value) {
                    return status;
                }
            }
            return IDLE;
        }
    }

    // Error code enum
    public enum ErrorCode {
        IDENTIFIER_NOT_DEFINED(0),
        VALUE_NOT_DEFINED(1),
        OUT_OF_RANGE(2),
        FILE_ERROR(3),
        CONFIG_ERROR(4),
        PRINTER_ERROR(5),
        SYNTAX_ERROR(6),
        MOVEMENT_ERROR(7),
        NO_ERROR(8);

        private final int value;

        ErrorCode(int value) {
            this.value = value;
        }

        public int getValue() {
            return value;
        }

        public static ErrorCode fromValue(int value) {
            for (ErrorCode error : values()) {
                if (error.value == value) {
                    return error;
                }
            }
            return NO_ERROR;
        }
    }

    // Log categories (matching C++ enum)
    public static class LogCategory {
        public static final int NONE = 0;
        public static final int ERROR = 1 << 0;
        public static final int WARNING = 1 << 1;
        public static final int INFO = 1 << 2;
        public static final int DEBUG = 1 << 3;
        public static final int MOTOR = 1 << 4;
        public static final int ENCODER = 1 << 5;
        public static final int BLUETOOTH = 1 << 6;
        public static final int PROFILE = 1 << 7;
        public static final int PERFORMANCE = 1 << 8;
        public static final int COMMAND = 1 << 9;
        public static final int ALL = 0xFFFFFFFF;
        public static final int DEFAULT = ERROR | WARNING | INFO | MOTOR | ENCODER | BLUETOOTH;
    }

    // Constructor
    public GCodeInterpreter(PrinterController printer) {
        NativeLib.ensureLoaded();
        if (printer == null) throw new IllegalArgumentException("Printer cannot be null");
        this.printerHandle = printer.getPrinterHandle();
        if (this.printerHandle == 0) throw new RuntimeException("Invalid printer handle");

        interpreterHandle = createInterpreter(this.printerHandle);
        if (interpreterHandle == 0) {
            throw new RuntimeException("Failed to create GCode interpreter instance");
        }
    }

    // ========== Native methods (declarations) ==========

    // Lifecycle management
    private native long createInterpreter(long printerHandle);
    private native void destroyInterpreter(long interpreterHandle);

    // Execution
    private native boolean executeGCode(long interpreterPtr, String filename, long printerHandle);
    private native boolean executeLine(long interpreterPtr, String line, long printerHandle);

    // Control
    private native void pauseExecution(long interpreterPtr);
    private native void resumeExecution(long interpreterPtr);

    // Status and info
    private native int getStatus(long interpreterPtr);
    private native double getProgress(long interpreterPtr);
    private native String getLastInterpreterError(long interpreterPtr);
    // Configuration
    private native boolean readConfig(long interpreterPtr, String filename);

    // ========== Public API (similar to C#) ==========
    public boolean executeFile(String filename, PrinterController printer) {
        checkDisposed();
        if (filename == null || filename.isEmpty()) {
            System.err.println("ERROR: Filename is null or empty");
            return false;
        }
        if (printer == null) {
            System.err.println("ERROR: Printer is null");
            return false;
        }

        long printerHandle = printer.getPrinterHandle();
        if (printerHandle == 0) {
            System.err.println("ERROR: Printer handle is invalid");
            return false;
        }

        try {
            // Get absolute path
            String fullPath = new java.io.File(filename).getAbsolutePath();
            System.out.println("Executing file: " + fullPath);

            // Execute g-code
            return executeGCode(interpreterHandle, fullPath, printerHandle);
        }
        catch(Exception ex) {
            System.err.println("Exception in executeFile: " + ex);
            return false;
        }
    }

    public boolean executeFile(String filename, long printerHandle) {
        checkDisposed();
        if (filename == null || filename.isEmpty()) {
            System.err.println("ERROR: Filename is null or empty");
            return false;
        }
        if (printerHandle == 0) {
            System.err.println("ERROR: Printer handle is invalid");
            return false;
        }

        try {
            String fullPath = new java.io.File(filename).getAbsolutePath();
            System.out.println("Executing file: " + fullPath);
            return executeGCode(interpreterHandle, fullPath, printerHandle);
        }
        catch(Exception ex) {
            System.err.println("Exception in executeFile: " + ex);
            return false;
        }
    }

    public boolean executeLine(String line, PrinterController printer) {
        checkDisposed();
        if (line == null || line.isEmpty()) {
            System.err.println("ERROR: Line is null or empty");
            return false;
        }
        if (printer == null) {
            throw new IllegalArgumentException("Printer cannot be null");
        }

        long printerHandle = printer.getPrinterHandle();
        if (printerHandle == 0) {
            System.err.println("ERROR: Printer handle is invalid");
            return false;
        }

        try {
            return executeLine(interpreterHandle, line, printerHandle);
        }
        catch(Exception ex) {
            System.err.println("Execption in executeLine: " + ex);
            throw  new RuntimeException(ex);
        }
    }

    public boolean executeLine(String line, long printerHandle) {
        checkDisposed();
        if (line == null || line.isEmpty()) {
            System.err.println("ERROR: Line is null or empty");
            return false;
        }
        if (printerHandle == 0) {
            System.err.println("ERROR: Printer handle is invalid");
            return false;
        }

        try {
            return executeLine(interpreterHandle, line, printerHandle);
        }
        catch(Exception ex) {
            System.err.println("Execption in executeLine: " + ex);
            throw new RuntimeException(ex);
        }
    }

    public void pause() {
        checkDisposed();
        pauseExecution(interpreterHandle);
    }

    public void resume() {
        checkDisposed();
        pauseExecution(interpreterHandle);
    }

    public Status getStatus() {
        checkDisposed();
        int statusValue = getStatus(interpreterHandle);
        return Status.fromValue(statusValue);
    }

    public double getProgress() {
        checkDisposed();
        return getProgress(interpreterHandle);
    }

    public String getLastError() {
        checkDisposed();
        String error = getLastInterpreterError(interpreterHandle);
        return error != null ? error : "";
    }

    public boolean readConfig(String filename) {
        checkDisposed();
        if (filename == null || filename.isEmpty()) {
            return false;
        }
        return readConfig(interpreterHandle, filename);
    }
    public boolean isRunning() {
        return getStatus() == Status.RUNNING;
    }

    public boolean isCompleted() {
        return getStatus() == Status.COMPLETED;
    }

    public boolean isPaused() {
        return getStatus() == Status.PAUSED;
    }

    public boolean isIdle() {
        return getStatus() == Status.IDLE;
    }

    // Get interpreter handle for direct native calls
    public long getInterpreterHandle() {
        return interpreterHandle;
    }

    private void checkDisposed() {
        if (disposed) {
            throw new IllegalStateException("GCodeInterpreter has been disposed");
        }
    }

    @Override
    public void close() {
        if (!disposed) {
            if (interpreterHandle != 0) {
                // Give time for proper completion
                // 10 attempts of 100 ms = 1 second
                for (int i = 0; i < 10; i++) {
                    if (!isRunning()) {
                        break;
                    }
                    try {
                        Thread.sleep(100);
                    }
                    catch(InterruptedException e) {
                        Thread.currentThread().interrupt();
                        break;
                    }
                }

                // Destroy the interpreter
                destroyInterpreter(interpreterHandle);
                interpreterHandle = 0;
            }
            disposed = true;
        }
    }

    @Override
    protected void finalize() throws Throwable {
        try {
            close();
        }
        finally {
            super.finalize();
        }
    }
}
