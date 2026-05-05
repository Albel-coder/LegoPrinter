package com.example.lpstudio.ble;

import android.bluetooth.*;
import android.bluetooth.le.*;
import android.content.Context;
import android.util.Log;

import java.util.UUID;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import android.annotation.SuppressLint;

@SuppressLint("MissingPermission")
public class BleManager {
    private static final String TAG = "BleManager";
    private static final String LEGO_HUB_SERVICE_UUID = "00001623-1212-efde-1623-785feabcd123";
    private static final String LEGO_HUB_CHARACTERISTIC_UUID = "00001624-1212-efde-1623-785feabcd123";
    private static final long SCAN_TIMEOUT_MS = 10000;
    private static final long CONNECT_TIMEOUT_MS = 10000;

    private Context context;
    private BluetoothAdapter bluetoothAdapter;
    private BluetoothLeScanner bleScanner;
    private BluetoothGatt bluetoothGatt;
    private BluetoothDevice targetDevice;

    private ScanCallback scanCallback;
    private BluetoothGattCallback gattCallback;

    private volatile boolean isConnected = false;
    private volatile boolean connectionInProgress = false;
    private final Object lock = new Object();

    // Callbacks from native code (set via JNI)
    private long nativeTransportPtr; // for callbacks

    public BleManager(Context context) {
        this.context = context.getApplicationContext();
        BluetoothManager manager = (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        bluetoothAdapter = manager.getAdapter();
        bleScanner = bluetoothAdapter.getBluetoothLeScanner();
    }

    public void setNativeTransportPtr(long ptr) {
        this.nativeTransportPtr = ptr;
    }

    public boolean open() {
        synchronized (lock) {
            if (isConnected || connectionInProgress) {
                Log.w(TAG, "open already connected or in progress");
                return false;
            }
            connectionInProgress = true;
        }

        Log.i(TAG, "open: starting scan for LEGO Hub...");
        final CountDownLatch latch = new CountDownLatch(1);
        final boolean[] resultHolder = new boolean[1];

        // Launch scanning in new thread
        new Thread(() -> {
            try {
                resultHolder[0] = scanAndConnect();
            }
            finally {
               latch.countDown();
            }
        }).start();

        try {
            boolean awaited = latch.await(SCAN_TIMEOUT_MS + CONNECT_TIMEOUT_MS + 2000, TimeUnit.MILLISECONDS);
            if (!awaited) {
                Log.e(TAG, "open: timeout");
                synchronized (lock) {
                    connectionInProgress = false;
                }
                stopScan();
                return false;
            }
        }
        catch(InterruptedException e) {
            Log.e(TAG, "open interrupted", e);
            synchronized (lock) {
                connectionInProgress = false;
            }
            return false;
        }

        synchronized (lock) {
            connectionInProgress = false;
            if (resultHolder[0]) {
                isConnected = true;
            }
        }
        return resultHolder[0];
    }

    private boolean scanAndConnect() {
        final CountDownLatch scanLatch = new CountDownLatch(1);
        final boolean[] deviceFound = new boolean[1];

        ScanCallback callback = new ScanCallback() {
            @Override
            public void onScanResult(int callbackType, ScanResult result) {
                BluetoothDevice device = result.getDevice();
                String name = device.getName();
                if (name == null) name = "";
                String upperName = name.toUpperCase();

                // Check by name
                boolean isLego = upperName.contains("LEGO") || upperName.contains("HUB") || upperName.contains("CONTROL");

                // Checking manufacturer data (LEGO Company ID 0x0397)
                ScanRecord record = result.getScanRecord();
                if (record != null) {
                    byte[] manufacturerData = record.getManufacturerSpecificData(0x0397);
                    if (manufacturerData != null && manufacturerData.length > 0) {
                        isLego = true;
                        Log.i(TAG, "LEGO Manufacturer Data found for " + name);
                    }
                }

                if (isLego) {
                    Log.i(TAG, "LEGO HUB DISCOVERED: " + name + " [" + device.getAddress() + "]");
                    targetDevice = device;
                    deviceFound[0] = true;
                    scanLatch.countDown();
                }
            }

            @Override
            public void onScanFailed(int errorCode) {
                Log.e(TAG, "Scan failed, error: " + errorCode);
                scanLatch.countDown();
            }
        };

        bleScanner.startScan(callback);
        this.scanCallback = callback;

        try {
            boolean found = scanLatch.await(SCAN_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            bleScanner.stopScan(callback);
            if (!found || targetDevice == null) {
                Log.e(TAG, "No LEGO Hub found within timeout");
                return false;
            }
        } catch (InterruptedException e) {
            Log.e(TAG, "Scan interrupted", e);
            bleScanner.stopScan(callback);
            return false;
        }

        // Connect to the found device
        Log.i(TAG, "Connecting to " + targetDevice.getAddress());
        final CountDownLatch connectLatch = new CountDownLatch(1);
        final boolean[] connectResult = new boolean[1];

        gattCallback = new BluetoothGattCallback() {
            @Override
            public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
                if (status == BluetoothGatt.GATT_SUCCESS && newState == BluetoothProfile.STATE_CONNECTED) {
                    Log.i(TAG, "Connected to GATT server, starting service discovery");
                    bluetoothGatt = gatt;
                    gatt.discoverServices();
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    Log.e(TAG, "Disconnected, status: " + status);
                    connectResult[0] = false;
                    connectLatch.countDown();
                }
            }

            @Override
            public void onServicesDiscovered(BluetoothGatt gatt, int status) {
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    // Check for the availability of the required service and characteristics
                    BluetoothGattService service = gatt.getService(UUID.fromString(LEGO_HUB_SERVICE_UUID));
                    if (service != null) {
                        BluetoothGattCharacteristic characteristic = service.getCharacteristic(UUID.fromString(LEGO_HUB_CHARACTERISTIC_UUID));
                        if (characteristic != null) {
                            Log.i(TAG, "Required service and characteristic found");
                            // Enable notifications
                            gatt.setCharacteristicNotification(characteristic, true);
                            // You can send a request to enable notifications via descriptor here if required
                            connectResult[0] = true;
                        } else {
                            Log.e(TAG, "Characteristic not found");
                            connectResult[0] = false;
                        }
                    } else {
                        Log.e(TAG, "Service not found");
                        connectResult[0] = false;
                    }
                } else {
                    Log.e(TAG, "Service discovery failed, status: " + status);
                    connectResult[0] = false;
                }
                connectLatch.countDown();
            }

            @Override
            public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
                // Data received from the device
                byte[] value = characteristic.getValue();
                if (value != null && nativeTransportPtr != 0) {
                    // Call a native method to transfer data in C++ (implemented in TransportAndroid)
                    nativeOnDataReceived(nativeTransportPtr, value);
                }
            }
        };

        bluetoothGatt = targetDevice.connectGatt(context, false, gattCallback);
        try {
            boolean connected = connectLatch.await(CONNECT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            if (!connected || !connectResult[0]) {
                Log.e(TAG, "Connection timeout or failed");
                bluetoothGatt.close();
                bluetoothGatt = null;
                return false;
            }
        } catch (InterruptedException e) {
            Log.e(TAG, "Connection interrupted", e);
            bluetoothGatt.close();
            bluetoothGatt = null;
            return false;
        }
        return true;
    }

    // ---------- close ----------
    public void close() {
        synchronized (lock) {
            if (bluetoothGatt != null) {
                bluetoothGatt.disconnect();
                bluetoothGatt.close();
                bluetoothGatt = null;
            }
            isConnected = false;
            connectionInProgress = false;
            targetDevice = null;
        }
    }

    // ---------- write ----------
    public boolean write(byte[] data) {
        if (bluetoothGatt == null) return false;
        BluetoothGattService service = bluetoothGatt.getService(UUID.fromString(LEGO_HUB_SERVICE_UUID));
        if (service == null) return false;
        BluetoothGattCharacteristic characteristic = service.getCharacteristic(UUID.fromString(LEGO_HUB_CHARACTERISTIC_UUID));
        if (characteristic == null) return false;
        characteristic.setValue(data);
        characteristic.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE);
        return bluetoothGatt.writeCharacteristic(characteristic);
    }

    // ---------- isConnected ----------
    public boolean isConnected() {
        return isConnected;
    }

    private void stopScan() {
        if (scanCallback != null) {
            bleScanner.stopScan(scanCallback);
            scanCallback = null;
        }
    }

    private native void nativeOnDataReceived(long nativeTransportPtr, byte[] data);
    private native void nativeOnConnectionStateChanged(long nativeTransportPtr, boolean connected);
}