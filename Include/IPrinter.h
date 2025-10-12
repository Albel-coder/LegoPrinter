#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// opaque pointer
typedef struct IPrinter IPrinter;

// struct for motor config
typedef struct
{
	unsigned char Port;
	signed char Speed;
	double Revolutions;
} MotorCommand;

// Virtual table of printer methods
typedef struct
{
	// Main operations
	bool (*Connect)(IPrinter* Self);
	bool (*Disconnect)(IPrinter* Self);
	bool (*IsConnected)(IPrinter* Self);
	void (*Destroy)(IPrinter* Self);

	// Motors control
	void (*RotateMotor)(IPrinter* Self, const MotorCommand* Commands, int Count);

	// Raw command
	void (*SendCommand)(IPrinter* Self, const unsigned char* Command, int Length);


} IPrinterVirtualTable;

// Main struct interface
struct IPrinter
{
	IPrinterVirtualTable* VirtualTable;
};

#ifdef __cplusplus
}
#endif