package com.example.lpstudio;

import java.util.ArrayList;
import java.util.List;

// Java wrapper for C++ printer driver
// C# PrinterController Analog
public class PrinterController implements AutoCloseable {

    // Pointer to the native IPrinter object (stored as long)
    private long printerHandle = 0;
    private boolean disposed = false;

    // Data structures (correspond to C structures)
    public static class MotorCommand {
        public byte port;
        public byte speed; // sbyte in C -> byte in Java
        public double revolutions;

        public MotorCommand() {}

        public MotorCommand(byte port, byte speed, double revolutions) {
            this.port = port;
            this.speed = speed;
            this.revolutions = revolutions;
        }
    }

    public static class SpeedProfilePoint {
        public double distance;
        public byte speed;
        public double tolerance;

        public SpeedProfilePoint() {}

        public SpeedProfilePoint(double distance, byte speed, double tolerance) {
            this.distance = distance;
            this.speed = speed;
            this.tolerance = tolerance;
        }

    }
    public static class SpeedProfile {
        public byte port;
        public SpeedProfilePoint[] points;
        public int timeoutMs;

        public SpeedProfile(byte port, SpeedProfilePoint[] points, int timeoutMs) {
            this.port = port;
            this.points = points;
            this.timeoutMs = timeoutMs;
        }
    }

    // Enums
    public static final class MotorMode {
        public static final int STOP = 0;
        public static final int CONST_SPEED = 1;
        public static final int POSITION = 2;
        public static final int PROFILE = 3;
    }

    // Constructor
    public PrinterController() {
        printerHandle = createPrinter();
        if (printerHandle == 0) {
            throw new RuntimeException("Failed to create printer instance");
        }
    }

    // ========== Native methods (declarations) ==========

    // Lifecycle management
    private native long createPrinter();
    private native void destroyPrinter(long printerPtr);

    // Connection
    private native boolean printerConnect(long printerPtr);
    private native boolean printerDisconnect(long printerPtr);
    private native boolean isConnected(long printerPtr);

    // Motor control
    private native void printerRotateMotor(long printerPtr, MotorCommand[] commands, int count);
    private native void printerSendCommand(long printerPtr, byte[] command, int length);
    private native void printerSetMotorSpeed(long printerPtr, byte port, byte speed);

    // Logging
    private native int getLogCount(long printerPtr);
    private native String getLogEntry(long printerPtr, int index);
    private native void clearLog(long printerPtr);
    private native String getLastErrorMessage(long printerPtr);
    private native String printerConnectionInfo(long printerPtr);

    // Log categories
    private native void printerSetLogCategories(long printerPtr, int categories);
    private native int printerGetLogCategories(long printerPtr);

    // Speed profiles
    private native boolean printerExecuteSpeedProfile(long printerPtr, SpeedProfile profile);
    private native boolean printerExecuteSpeedProfiles(long printerPtr, SpeedProfile[] profiles, int count);

    // Monitoring
    private native boolean printerIsMotorMoving(long printerPtr);
    private native double printerGetMotorPosition(long printerPtr, byte port);

    // Testing
    private native boolean runPrinterTest(long printerPtr, String testName);

    // Battery
    private native boolean printerRequestBatteryLevel(long printerPtr);
    private native byte printerGetBatteryLevel(long printerPtr);
    private native boolean printerIsBatteryLevelFresh(long printerPtr, int maxAgeSeconds);

    // =========== Public API (similar to C#) ==========

    public boolean connect() {
        checkDisposed();
        return printerConnect(printerHandle);
    }

    public boolean disconnect() {
        checkDisposed();
        return printerDisconnect(printerHandle);
    }

    public boolean isConnected() {
        checkDisposed();
        return isConnected(printerHandle);
    }

    public void rotateMotor(MotorCommand[] commands) {
        checkDisposed();
        if (commands == null || commands.length == 0) {
            return;
        }
        printerRotateMotor(printerHandle, commands, commands.length);
    }

    public void sendCommand(byte[] command) {
        checkDisposed();
        if (command == null || command.length == 0) {
            return;
        }
        printerSendCommand(printerHandle, command, command.length);
    }

    public void setMotorSpeed(byte port, byte speed) {
        checkDisposed();
        printerSetMotorSpeed(printerHandle, port, speed);
    }

    public int getLogCount() {
        checkDisposed();
        return getLogCount(printerHandle);
    }

    public String getLogEntry(int index) {
        checkDisposed();
        return getLogEntry(printerHandle, index);
    }

    public List<String> getAllLogs() {
        checkDisposed();
        List<String> logs = new ArrayList<>();
        int count = getLogCount();
        for (int i = 0; i < count; i++) {
            String entry = getLogEntry(i);
            if (entry != null && !entry.isEmpty()) {
                logs.add(entry);
            }
        }
        return logs;
    }

    public void clearLog() {
        checkDisposed();
        clearLog(printerHandle);
    }

    public String getLastErrorMessage() {
        checkDisposed();
        return getLastErrorMessage(printerHandle);
    }

    public String getConnectionInfo() {
        checkDisposed();
        return printerConnectionInfo(printerHandle);
    }

    public void setLogCategories(int categories) {
        checkDisposed();
        printerSetLogCategories(printerHandle, categories);
    }

    public int getLogCategories() {
        checkDisposed();
        return printerGetLogCategories(printerHandle);
    }

    public boolean executeSpeedProfile(SpeedProfile profile) {
        checkDisposed();
        return printerExecuteSpeedProfile(printerHandle, profile);
    }

    public boolean executeSpeedProfiles(SpeedProfile[] profiles) {
        checkDisposed();
        if (profiles == null || profiles.length == 0) {
            return false;
        }
        return printerExecuteSpeedProfiles(printerHandle, profiles, profiles.length);
    }

    public boolean isMotorMoving() {
        checkDisposed();
        return printerIsMotorMoving(printerHandle);
    }

    public double getMotorPosition(byte port) {
        checkDisposed();
        return printerGetMotorPosition(printerHandle, port);
    }

    public boolean runTest(String testName) {
        checkDisposed();
        return runPrinterTest(printerHandle, testName);
    }

    public boolean getBatteryLevel() {
        checkDisposed();
        return printerRequestBatteryLevel(printerHandle);
    }

    public boolean isBatteryLevelFresh(int maxAgeSeconds) {
        checkDisposed();
        return printerIsBatteryLevelFresh(printerHandle, maxAgeSeconds);
    }

    public void test() {
        checkDisposed();
        try {
            MotorCommand[] commands = new MotorCommand[2];
            commands[0] = new MotorCommand((byte)0x02, (byte)20, 3.0);
            commands[1] = new MotorCommand((byte)0x03, (byte)20, 3.0);

            MotorCommand[] x = new MotorCommand[1];
            x[0] = new MotorCommand((byte)0x00, (byte)-10, 3.0);
            rotateMotor(x);
        }
        catch (Exception e) {
            System.err.println("Test error: " + e);
        }
    }

    public long getPrinterHandle() {
        return printerHandle;
    }

    private void checkDisposed() {
        if (disposed) {
            throw new IllegalStateException("PrinterController has been disposed");
        }
    }
    @Override
    public void close() {
        if (!disposed) {
            if (printerHandle != 0) {
                destroyPrinter(printerHandle);
                printerHandle = 0;
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
