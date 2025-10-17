using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using static PrinterController;

// Class for managing a printer
public class PrinterController : IDisposable
{

    // Data structures for interfacing with C++ DLLs

    [StructLayout(LayoutKind.Sequential)]
    public struct IPrinter
    {
        public IntPtr VirtualTable;
    }

    private bool Disposed = false;
    private IPrinter PrinterHandle;
    private readonly object SyncRoot = new object();

    [StructLayout(LayoutKind.Sequential)]
    public struct MotorCommand
    {
        public byte Port;
        public sbyte Speed;
        public double Revolutions;
    }
    public PrinterController()
    {
        PrinterHandle = CreatePrinter();
        if (PrinterHandle.VirtualTable == IntPtr.Zero)
        {
            throw new InvalidOperationException("Failed to create printer instance");
        }
    }

    public void RotateMotor(MotorCommand[] Commands)
    {
        if (Commands == null || Commands.Length == 0)
        {
            return;
        }

        lock (SyncRoot)
        {
            if (Disposed)
            {
                throw new ObjectDisposedException("PrinterController");
            }

            SafeCall(() =>
            {
                PrinterRotateMotor(PrinterHandle, Commands, Commands.Length);
                return true;
            }, false);
        }
    }
    public void Test()
    {
        lock (SyncRoot)
        {
            try
            {
                MotorCommand[] Command = new MotorCommand[2];

                Command[0].Port = 0x02;
                Command[0].Speed = 20;
                Command[0].Revolutions = 3;

                Command[1].Port = 0x03;
                Command[1].Speed = 20;
                Command[1].Revolutions = 3;

                RotateMotor(Command);

                MotorCommand[] X = new MotorCommand[1];

                X[0].Port = 0x00;
                X[0].Speed = -10;
                X[0].Revolutions = 3;

                RotateMotor(X);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"TEST ERROR: {ex}");
            }
        }
    }

    public IPrinter GetPrinterHandle()
    {
        if (Disposed)
        {
            throw new ObjectDisposedException("PrinterController", "Cannot access printer after disposal");
        }

        if (PrinterHandle.VirtualTable == IntPtr.Zero)
        {
            throw new InvalidOperationException("Printer handle is not valid");
        }

        return PrinterHandle;
    }

    public int GetLogCount() => SafeCall(() => GetLogCount(PrinterHandle), 0);

    public string GetLogEntry(int Index)
    {
        return SafeCall(() =>
        {
            IntPtr logEntryPtr = GetLogEntry(PrinterHandle, Index);
            return logEntryPtr != IntPtr.Zero ? Marshal.PtrToStringAnsi(logEntryPtr) : string.Empty;
        }, string.Empty);
    }

    public List<string> GetAllLogs()
    {
        var Logs = new List<string>();
        int Count = GetLogCount();
        for (int i = 0; i < Count; i++)
        {
            string LogEntry = GetLogEntry(i);
            if (!string.IsNullOrEmpty(LogEntry))
            {
                Logs.Add(LogEntry);
            }
        }

        return Logs;
    }

    public void ClearLog() => SafeCall(() => { ClearLog(PrinterHandle); return true; }, false);

    public string GetLastError()
    {
        return SafeCall(() =>
        {
            IntPtr ErrorPtr = GetLastErrorMsg(PrinterHandle);
            return ErrorPtr != IntPtr.Zero ? Marshal.PtrToStringAnsi(ErrorPtr) : string.Empty;
        }, string.Empty);
    }

    public void PrintConnectionInfo() => SafeCall(() => { PrintConnectionInfo(PrinterHandle); return true; }, false);
    public bool Connect() => SafeCall(() => PrinterConnect(PrinterHandle), false);
    public bool Disconnect() => SafeCall(() => PrinterDisconnect(PrinterHandle), false);

    // Helper method for safely calling a DLL function
    private T SafeCall<T>(Func<T> function, T DefaultValue = default(T))
    {
        if (Disposed)
        {
            return DefaultValue;
        }

        return function();
    }

    // Importing functions from DLL

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IPrinter CreatePrinter();

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void DestroyPrinter(IPrinter printer);

    // Connection functions
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterConnect(IPrinter printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterDisconnect(IPrinter printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterIsConnected(IPrinter printer);

    // Functions for controlling motors

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterRotateMotor(IPrinter printer,
        [MarshalAs(UnmanagedType.LPArray)] MotorCommand[] commands, int count);

    // Commands
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterSendCommand(IPrinter printer,
        [MarshalAs(UnmanagedType.LPArray)] byte[] data, int length);

    // System Logging
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int GetLogCount(IPrinter printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr GetLogEntry(IPrinter printer, int index);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void ClearLog(IPrinter printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr GetLastErrorMsg(IPrinter printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrintConnectionInfo(IPrinter printer);

    // To properly release resources
    public void Dispose()
    {
        if (!Disposed)
        {
            if (PrinterHandle.VirtualTable != IntPtr.Zero)
            {
                DestroyPrinter(PrinterHandle);
                PrinterHandle.VirtualTable = IntPtr.Zero;
            }

            Disposed = true;
        }
    }
}

public class GCodeInterpreter : IDisposable
{
    private IntPtr InterpreterHandle;
    private bool Disposed = false;

    public GCodeInterpreter()
    {
        InterpreterHandle = CreateInterpreter();
        if (InterpreterHandle == IntPtr.Zero)
        {
            throw new InvalidOperationException("Failed to create GCode interpreter");
        }
    }

    public bool Test(IPrinter printer)
    {
        return TestCode(InterpreterHandle, printer);
    }

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr CreateInterpreter();

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void DestroyInterpreter(IntPtr interpreter);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool TestCode(IntPtr interpreter, IPrinter printer);
    public void Dispose()
    {
        if (!Disposed)
        {
            DestroyInterpreter(InterpreterHandle);
            InterpreterHandle = IntPtr.Zero;
            Disposed = true;
        }
    }
}