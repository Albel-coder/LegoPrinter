#include "TransportSimpleBLE.h"
#include <chrono>
#include <thread>
#include <algorithm>

using namespace std::chrono_literals;

const std::string TransportSimpleBLE::LEGO_HUB_SERVICE_UUID = "00001623-1212-efde-1623-785feabcd123";
const std::string TransportSimpleBLE::LEGO_HUB_CHARACTERISTIC_UUID = "00001624-1212-efde-1623-785feabcd123";

TransportSimpleBLE::TransportSimpleBLE() {
    std::lock_guard<std::mutex> lock(stateMutex_);  
}

TransportSimpleBLE::~TransportSimpleBLE() {
    close();
    cleanup();
}

void TransportSimpleBLE::cleanup() {
    stopRequested_ = true;

    // Wait for the worker thread to complete
    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    std::lock_guard<std::mutex> lock(stateMutex_);
    threadRunning_ = false;
}

bool TransportSimpleBLE::open() {
    try {
        std::lock_guard<std::mutex> lock(stateMutex_);

        // Clear the previous state
        if (workerThread_.joinable()) {
            workerThread_.join();
        }

        // Reset flags
        stopRequested_ = false;
        threadRunning_ = false;

        workerThread_ = std::thread(&TransportSimpleBLE::workerFunction, this);
        return true;
    }
    catch (...) {        
        return false;
    }
}

void TransportSimpleBLE::workerFunction() {
    threadRunning_ = true;
    bool connected = false;

    try {

        // Checking Bluetooth Status
        LOG_BLUETOOTH("Checking Bluetooth status:");

        bool bleEnabled = SimpleBLE::Adapter::bluetooth_enabled();
        LOG_BLUETOOTH("  - SimpleBLE::Adapter::bluetooth_enabled(): %s",
            bleEnabled ? "true" : "false");

        // Getting a list of adapters
        auto adapters = SimpleBLE::Adapter::get_adapters();
        LOG_BLUETOOTH("  - Adapters found: %zu", adapters.size());

        if (adapters.empty()) {
            LOG_ERROR("Bluetooth adapters not found! Possible reasons:");
            LOG_ERROR("1. The Bluetooth adapter is disabled or not working");
            LOG_ERROR("2. Drivers not installed");
            LOG_ERROR("3. Hardware problem");
            if (connectionCallback_) connectionCallback_(false);
            return;
        }

        // We use the first adapter
        SimpleBLE::Adapter adapter = adapters[0];
        LOG_BLUETOOTH("Adapter used: %s [%s]",
            adapter.identifier().c_str(),
            adapter.address().c_str());

        deviceFound_ = false;
        {
            std::lock_guard<std::mutex> lock(candidateMutex_);
            peripheralCandidate_.reset();
        }

        // Setting up callbacks
        adapter.set_callback_on_scan_start([]() {});
        LOG_BLUETOOTH("Scanning started...");

        adapter.set_callback_on_scan_stop([]() {});
        LOG_BLUETOOTH("Scanning stopped");

        adapter.set_callback_on_scan_found([this](SimpleBLE::Peripheral peripheral) {
            std::string name = peripheral.identifier();
            std::string address = peripheral.address();
            int rssi = peripheral.rssi();

            // Convert the name to uppercase for universality
            std::transform(name.begin(), name.end(), name.begin(), ::toupper);

            // Check by name
            bool isLego = (name.find("LEGO") != std::string::npos) ||
                (name.find("HUB") != std::string::npos) ||
                (name.find("CONTROL") != std::string::npos);

            // Check with manufacturer data (LEGO Company ID: 0x0397)
            auto manufacturer_data = peripheral.manufacturer_data();
            for (const auto& data : manufacturer_data) {
                // LEGO Company ID: 0x0397 (little-endian: 97 03)
                if (data.first == 0x0397) {
                    isLego = true;
                    LOG_BLUETOOTH("[LEGO Manufacturer Data Found]");
                }
            }

            if (isLego) {
                LOG_BLUETOOTH("LEGO HUB DISCOVERED!");

                if (!deviceFound_.load()) {
                    std::lock_guard<std::mutex> lock(candidateMutex_);

                    if (!deviceFound_.load()) {
                        peripheralCandidate_ = std::move(peripheral);
                        deviceFound_ = true;
                    }
                }
            }
            });

        // Start scanning
        adapter.scan_start();
        LOG_BLUETOOTH("Starting Bluetooth scan...");

        auto startTime = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::seconds(10);

        while (!stopRequested_ && !deviceFound_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            auto elapsed = std::chrono::steady_clock::now() - startTime;

            if (elapsed >= timeout) {
                LOG_BLUETOOTH("Scan timeout reached (10 seconds)");
                break;
            }
        }
        
        adapter.scan_stop();

        // We get a list of found devices
        auto peripherals = adapter.scan_get_results();
        LOG_BLUETOOTH("Scan completed. Found %d devices", peripherals.size());

        // Search LEGO Hub
        if (deviceFound_.load()) {
            SimpleBLE::Peripheral candidate;
            {
                std::lock_guard<std::mutex> lock(candidateMutex_);
                if (peripheralCandidate_.has_value()) {
                    candidate = std::move(*peripheralCandidate_);
                }
                else {
                    LOG_ERROR("Device found but candidate is empty");
                    if (connectionCallback_) {
                        connectionCallback_(false);
                    }
                    return;
                }
            }

            std::string name = candidate.identifier();
            LOG_BLUETOOTH("Attempting to connect to LEGO Hub: %s", name.c_str());

            try {
                candidate.connect();

                if (candidate.is_connected()) {
                    LOG_BLUETOOTH("Successfully connected to LEGO Hub");

                    bool foundChar = false;
                    for (auto& service : candidate.services()) {
                        if (service.uuid() == LEGO_HUB_SERVICE_UUID) {
                            for (auto& characteristic : service.characteristics()) {
                                if (characteristic.uuid() == LEGO_HUB_CHARACTERISTIC_UUID) {
                                    foundChar = true;
                                    break;
                                }
                            }
                            break;
                        }
                    }

                    if (!foundChar) {
                        LOG_ERROR("Required service/characteristic not found");
                        candidate.disconnect();
                        if (connectionCallback_) {
                            connectionCallback_(false);
                        }
                        return;
                    }

                    peripheral_ = std::move(candidate);

                    peripheral_.notify(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID,
                        [this](const std::vector<uint8_t>& data) {
                            this->onNotification(data);
                        });

                    if (connectionCallback_) {
                        connectionCallback_(true);
                    }
                }
                else {
                    LOG_ERROR("Failed to connect (is_connected false)");
                    if (connectionCallback_) {
                        connectionCallback_(false);
                    }
                }
            }
            catch (const std::exception& e) {
                LOG_ERROR("Connection error: %s", e.what());
                if (connectionCallback_) {
                    connectionCallback_(false);
                }
            }
        }
        else {
            if (!stopRequested_) {
                LOG_ERROR("No LEGO Hub found within timeout");
                if (connectionCallback_) {
                    connectionCallback_(false);
                }
            }
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Worker thread exception: %s", e.what());
        if (connectionCallback_) connectionCallback_(false);
    }
    catch (...) {
        LOG_ERROR("Worker thread unknown exception");
        if (connectionCallback_) connectionCallback_(false);
    }

    threadRunning_ = false;
}

bool TransportSimpleBLE::close() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (!peripheral_.initialized() || peripheral_.address().empty()) {
        return false;
    }

    if (peripheral_.is_connected()) {
        try {
            peripheral_.disconnect();
        }
        catch (...) {
            // Ignoring disconnection errors
            return false;
        }
    }

    if (connectionCallback_) {
        connectionCallback_(false);
    }

    return true;
}

bool TransportSimpleBLE::write(const uint8_t* data, size_t length) {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (!peripheral_.is_connected()) {
        return false;
    }

    try {
        std::vector<uint8_t> buffer(data, data + length);
        peripheral_.write_command(LEGO_HUB_SERVICE_UUID,
            LEGO_HUB_CHARACTERISTIC_UUID,
            buffer);
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool TransportSimpleBLE::isConnected() {
    try {
        return peripheral_.is_connected();
    }
    catch (...) {
        return false;
    }
}

void TransportSimpleBLE::setDataCallback(std::function<void(const uint8_t*, size_t)> callback) {
    dataCallback_ = std::move(callback);
}

void TransportSimpleBLE::setConnectionCallback(std::function<void(bool)> callback) {
    connectionCallback_ = std::move(callback);
}

void TransportSimpleBLE::onNotification(const std::vector<uint8_t>& data) {
    if (dataCallback_) {
        dataCallback_(data.data(), data.size());
    }
}
