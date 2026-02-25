#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <memory>

class ITransport {
public:
	virtual ~ITransport() = default;

	virtual bool open() = 0;

	virtual bool close() = 0;

	virtual bool write(const uint8_t* data, size_t length) = 0;

	virtual bool isConnected() = 0;

	virtual void setDataCallback(std::function<void(const uint8_t*, size_t)> callback) = 0;

	virtual void setConnectionCallback(std::function<void(bool)> callback) = 0;

	virtual const char* getName() const = 0;
};

using TransportPtr = std::unique_ptr<ITransport>;