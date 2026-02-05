#include "TransportSimpleBLE.h"
#include <chrono>
#include <thread>
#include <algorithm>

using namespace std::chrono_literals;

const std::string TransportSimpleBLE::LEGO_HUB_SERVICE_UUID = "00001623-1212-efde-1623-785feabcd123";
const std::string TransportSimpleBLE::LEGO_HUB_CHARACTERISTIC_UUID = "00001624-1212-efde-1623-785feabcd123";

TransportSimpleBLE::TransportSimpleBLE() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    currentState_ = State::Disconnected;
}

TransportSimpleBLE::~TransportSimpleBLE() {
    close();
    cleanup();
}

void TransportSimpleBLE::cleanup() {
    stopRequested_ = true;

    // Ожидаем завершения рабочего потока
    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    std::lock_guard<std::mutex> lock(stateMutex_);
    currentState_ = State::Disconnected;
    threadRunning_ = false;
}

bool TransportSimpleBLE::open() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    // Проверяем состояние
    if (currentState_ != State::Disconnected && currentState_ != State::Error) {
        std::cerr << "TransportSimpleBLE: Already connecting or connected" << std::endl;
        return false;
    }

    // Очищаем предыдущее состояние
    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    // Сбрасываем флаги
    stopRequested_ = false;
    threadRunning_ = false;

    // Запускаем новый поток
    try {
        workerThread_ = std::thread(&TransportSimpleBLE::workerFunction, this);
        setState(State::Scanning);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "TransportSimpleBLE: Failed to start worker thread: " << e.what() << std::endl;
        setState(State::Error);
        return false;
    }
}

void TransportSimpleBLE::workerFunction() {
    threadRunning_ = true;

    try {
        // Инициализация адаптера
        auto adapters = SimpleBLE::Adapter::get_adapters();
        if (adapters.empty()) {
            throw std::runtime_error("No Bluetooth adapters found");
        }

        adapter_ = adapters[0];

        // Сканирование
        setState(State::Scanning);
        adapter_.scan_start();

        const auto scanTimeout = 10000ms;
        auto scanStart = std::chrono::steady_clock::now();
        bool found = false;

        while (!stopRequested_ &&
            std::chrono::steady_clock::now() - scanStart < scanTimeout) {

            auto peripherals = adapter_.scan_get_results();
            for (auto& p : peripherals) {
                std::string name = p.identifier();
                std::transform(name.begin(), name.end(), name.begin(), ::toupper);

                if (name.find("LEGO") != std::string::npos ||
                    name.find("HUB") != std::string::npos) {
                    peripheral_ = p;
                    found = true;
                    break;
                }
            }

            if (found) break;
            std::this_thread::sleep_for(500ms);
        }

        adapter_.scan_stop();

        if (!found || stopRequested_) {
            setState(State::Error);
            return;
        }

        // Подключение
        setState(State::Connecting);
        peripheral_.connect();

        if (!peripheral_.is_connected()) {
            throw std::runtime_error("Failed to connect to peripheral");
        }

        // Настройка уведомлений
        peripheral_.notify(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID,
            [this](const std::vector<uint8_t>& data) {
                this->onNotification(data);
            });

        // Успех
        setState(State::Connected);

        // Вызываем callback из рабочего потока
        if (connectionCallback_) {
            connectionCallback_(true);
        }

        // Ждем завершения
        while (!stopRequested_ && getState() == State::Connected) {
            std::this_thread::sleep_for(100ms);
        }

    }
    catch (const std::exception& e) {
        std::cerr << "TransportSimpleBLE worker error: " << e.what() << std::endl;
        setState(State::Error);

        if (connectionCallback_) {
            connectionCallback_(false);
        }
    }

    threadRunning_ = false;
}

void TransportSimpleBLE::close() {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (currentState_ == State::Disconnected || currentState_ == State::Disconnecting) {
            return;
        }
        setState(State::Disconnecting);
    }

    try {
        if (peripheral_.is_connected()) {
            peripheral_.disconnect();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "TransportSimpleBLE close error: " << e.what() << std::endl;
    }

    setState(State::Disconnected);

    if (connectionCallback_) {
        connectionCallback_(false);
    }
}

bool TransportSimpleBLE::write(const uint8_t* data, size_t length) {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (currentState_ != State::Connected || !peripheral_.is_connected()) {
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
        std::cerr << "TransportSimpleBLE write failed: " << e.what() << std::endl;
        return false;
    }
}

bool TransportSimpleBLE::isConnected() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return currentState_ == State::Connected && peripheral_.is_connected();
}

void TransportSimpleBLE::onNotification(const std::vector<uint8_t>& data) {
    if (dataCallback_) {
        dataCallback_(data.data(), data.size());
    }
}

void TransportSimpleBLE::setState(State newState) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    currentState_ = newState;
    stateCV_.notify_all();
}

TransportSimpleBLE::State TransportSimpleBLE::getState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return currentState_;
}

bool TransportSimpleBLE::waitForState(State state, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(stateMutex_);
    return stateCV_.wait_for(lock, timeout, [this, state] {
        return currentState_ == state || stopRequested_;
        });
}
