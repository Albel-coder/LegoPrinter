#pragma once

#ifdef LEGOPRINTERCORE_EXPORTS
#define PRINTER_DRIVER_API __declspec(dllexport)
#else
#define PRINTER_DRIVER_API __declspec(dllimport)
#endif

#include <simpleble/SimpleBLE.h>
#include <vector>
#include <thread>
#include <algorithm>

#include <fstream>

void SendCommand(SimpleBLE::Peripheral& peripheral, const std::vector<uint8_t>& command);

void UnlockAndSetLedColor(SimpleBLE::Peripheral& peripheral, uint8_t r, uint8_t g, uint8_t b);

void TestSpecialSequences(SimpleBLE::Peripheral& peripheral);

void TestAllLedCommands(SimpleBLE::Peripheral& peripheral);

void SetLedColor(SimpleBLE::Peripheral& peripheral, uint8_t r, uint8_t g, uint8_t b);

void SetLedColorSystem(SimpleBLE::Peripheral& peripheral, uint8_t r, uint8_t g, uint8_t b);

void SetLedColorAltPort(SimpleBLE::Peripheral& peripheral, uint8_t r, uint8_t g, uint8_t b);

void SetMotorSpeed(SimpleBLE::Peripheral& peripheral, uint8_t port, int8_t speed);

void RotateMotor(SimpleBLE::Peripheral& peripheral, 
    const uint8_t port, 
    int8_t speed,
    double revolutions);

void Connect();

void Test(SimpleBLE::Peripheral peripheral);