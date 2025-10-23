using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Ports;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
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
            IntPtr ErrorPtr = GetLastErrorMessage(PrinterHandle);
            return ErrorPtr != IntPtr.Zero ? Marshal.PtrToStringAnsi(ErrorPtr) : string.Empty;
        }, string.Empty);
    }

    public void PrintConnectionInfo() => SafeCall(() => { PrinterConnectionInfo(PrinterHandle); return true; }, false);
    public bool Connect() => SafeCall(() => PrinterConnect(PrinterHandle), false);
    public bool IsPrinterConnect() => SafeCall(() => IsConnected(PrinterHandle), false);
    public bool Disconnect() => SafeCall(() => PrinterDisconnect(PrinterHandle), false);

    // Helper method for safely calling a DLL function
    private T SafeCall<T>(Func<T> function, T DefaultValue = default(T))
    {
        if (Disposed)
        {
            return DefaultValue;
        }

        try
        {
            return function();
        }
        catch(Exception ex)
        {
            Console.WriteLine($"SafeCall exception: {ex}");
            return DefaultValue;
        }
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
    private static extern bool IsConnected(IPrinter printer);

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
    private static extern IntPtr GetLastErrorMessage(IPrinter printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterConnectionInfo(IPrinter printer);

    // To properly release resources
    public void Dispose()
    {
        if (!Disposed)
        {
            try
            {
                Console.WriteLine("Disposing PrinterController...");

                System.Threading.Thread.Sleep(100);

                if (PrinterHandle.VirtualTable != IntPtr.Zero)
                {
                    DestroyPrinter(PrinterHandle);
                    PrinterHandle.VirtualTable = IntPtr.Zero;
                }

                Disposed = true;
                Console.WriteLine("PrinterController disposed successfully");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error during printer disposal: {ex}");
            }
        }
    }
}

public class GCodeInterpreter : IDisposable
{
    private IntPtr InterpreterHandle;
    private bool Disposed = false;

    public enum Status
    {
        IDLE = 0,
        CHECKING_CODE = 1,
        RUNNING = 2,
        PAUSED = 3,
        COMPLETED = 4,
        ERROR = 5
    }

    public enum ErrorCode
    {
        IDENTIFIER_NOT_DEFINED = 0,
        VALUE_NOT_DEFINED = 1,
        OUT_OF_RANGE = 2,
        FILE_ERROR = 3,
        CONFIG_ERROR = 4,
        PRINTER_ERROR = 5,
        SYNTAX_ERROR = 6,
        MOVEMENT_ERROR = 7,
        NO_ERROR = 8
    }

    public GCodeInterpreter()
    {
        InterpreterHandle = CreateInterpreter();
        if (InterpreterHandle == IntPtr.Zero)
        {
            throw new InvalidOperationException("Failed to create GCode interpreter");
        }
    }

    // Основные методы
    public bool Test(IPrinter printer) => TestCode(InterpreterHandle, printer);

    // ИСПРАВЛЕННЫЙ метод - правильный порядок параметров
    public bool ExecuteFile(string filename, IPrinter printer)
    {
        if (string.IsNullOrEmpty(filename))
        {
            Console.WriteLine("C#: ERROR - Filename is null or empty");
            return false;
        }

        if (printer.VirtualTable == IntPtr.Zero)
        {
            Console.WriteLine("C#: ERROR - Printer VirtualTable is zero");
            return false;
        }

        try
        {
            string fullPath = Path.GetFullPath(filename);
            Console.WriteLine($"C#: Executing file '{fullPath}'");
            Console.WriteLine($"C#: File exists: {File.Exists(fullPath)}");

            // ПРАВИЛЬНЫЙ ПОРЯДОК ПАРАМЕТРОВ: interpreter, filename, printer
            return ExecuteGcode(InterpreterHandle, fullPath, printer);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"C#: Exception in ExecuteFile: {ex}");
            return false;
        }
    }
    public void Pause() => PauseExecution(InterpreterHandle);
    public void Resume() => ResumeExecution(InterpreterHandle);
    public Status GetStatus() => (Status)GetStatus(InterpreterHandle);
    public double GetProgress() => GetProgress(InterpreterHandle);

    // Строковые методы - ТЕПЕРЬ КАК В ДРАЙВЕРЕ!
    public string GetLastError()
    {
        IntPtr errorPtr = GetLastInterpreterError(InterpreterHandle);
        return errorPtr != IntPtr.Zero ? Marshal.PtrToStringAnsi(errorPtr) : string.Empty;
    }

    public string GetError(int index)
    {
        IntPtr errorPtr = GetError(InterpreterHandle, index);
        return errorPtr != IntPtr.Zero ? Marshal.PtrToStringAnsi(errorPtr) : string.Empty;
    }

    public string GetLog(int index)
    {
        IntPtr logPtr = GetLogEntry(InterpreterHandle, index);
        return logPtr != IntPtr.Zero ? Marshal.PtrToStringAnsi(logPtr) : string.Empty;
    }

    public int GetErrorCount() => GetErrorCount(InterpreterHandle);
    public int GetLogCount() => GetLogCount(InterpreterHandle);
    public void ClearErrors() => ClearErrors(InterpreterHandle);
    public void ClearLog() => ClearLog(InterpreterHandle);
    public bool ReadConfig(string filename) => ReadConfing(InterpreterHandle, filename);

    // Вспомогательные методы для С#
    public List<string> GetAllErrors()
    {
        var Errors = new List<string>();
        int Count = GetErrorCount();
        for (int i = 0; i < Count; i++)
        {
            string error = GetError(i);
            if (!string.IsNullOrEmpty(error))
            {
                Errors.Add(error);
            }
        }
        return Errors;
    }

    public List<string> GetAllLogs()
    {
        var Logs = new List<string>();
        int Count = GetLogCount();
        for (int i = 0; i < Count; i++)
        {
            string log = GetLog(i);
            if (!string.IsNullOrEmpty(log))
            {
                Logs.Add(log);
            }
        }
        return Logs;
    }

    public bool HasErrors => GetErrorCount() > 0;
    public bool IsRunning => GetStatus() == Status.RUNNING;
    public bool IsCompleted => GetStatus() == Status.COMPLETED;
    public bool IsError => GetStatus() == Status.ERROR;

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr CreateInterpreter();

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void DestroyInterpreter(IntPtr interpreter);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool TestCode(IntPtr interpreter, IPrinter printer);

    // ИСПРАВЛЕННЫЙ DllImport с правильным порядком параметров
    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool ExecuteGcode(IntPtr interpreter, string filename, IPrinter printer);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PauseExecution(IntPtr interpreter);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void ResumeExecution(IntPtr interpreter);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int GetStatus(IntPtr interpreter);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern double GetProgress(IntPtr interpreter);

    // ИЗМЕНЕНО: теперь возвращают IntPtr вместо string
    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr GetLastInterpreterError(IntPtr interpreter);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int GetErrorCount(IntPtr interpreter);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr GetError(IntPtr interpreter, int index);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int GetLogCount(IntPtr interpreter);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr GetLogEntry(IntPtr interpreter, int index);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void ClearErrors(IntPtr interpreter);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void ClearLog(IntPtr interpreter);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool ReadConfing(IntPtr interpreter, string filename);

    public void Dispose()
    {
        if (!Disposed && InterpreterHandle != IntPtr.Zero)
        {
            try
            {
                Console.WriteLine("Disposing GCodeInterpreter...");

                // Даем время на корректное завершение
                for (int i = 0; i < 10; i++) // 10 попыток по 100 мс = 1 секунда
                {
                    if (!IsRunning)
                    {
                        break;
                    }

                    System.Threading.Thread.Sleep(100);
                }

                // Уничтожаем интерпретатор
                DestroyInterpreter(InterpreterHandle);
                InterpreterHandle = IntPtr.Zero;
                Disposed = true;

                Console.WriteLine("GCodeInterpreter disposed successfully");
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
                
                // Все равно помечаем как disposed
                Disposed = true;
            }
        }
    }
}
