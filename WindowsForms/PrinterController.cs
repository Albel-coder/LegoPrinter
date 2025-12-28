using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Ports;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
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

    [StructLayout(LayoutKind.Sequential)]
    public struct MotorCommand
    {
        public byte Port;
        public sbyte Speed;
        public double Revolutions;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MotorCommandExe
    {
        public MotorMode Mode;
        public sbyte Speed;
        public double TargetRevolutions;
        public sbyte MaxSpeed;
        public ProfileData Profile;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ProfileData
    {
        public sbyte StartSpeed;
        public sbyte EndSpeed;
        public double Acceleration;
        public double Distance;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct CommandStream
    {
        public byte Port;
        public IntPtr Commands; // Pointer to MotorCommandExe array
        public int Count;
        public uint Timestamp;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EncoderEvent
    {
        public byte Port;
        public EncoderEventType Type;
        public double TargetPosition;
        public double Tolerance;
        public IntPtr Callback;
        public IntPtr UserData;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SpeedProfilePoint
    {
        public double Position;
        public sbyte Speed;
        public double Tolerance;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SpeedProfile
    {
        public byte Port;
        public IntPtr Points; // Pointer to SpeedProfilePoint array
        public int Count;
        public int TimeoutMs;
    }

    // Enums
    public enum MotorMode
    {
        STOP = 0,
        CONST_SPEED = 1,
        POSITION = 2,
        PROFILE = 3
    }
    public enum EncoderEventType
    {
        ENCODER_POSITION_REACHED = 0,
        ENCODER_SEGMENT_COMPLETED = 1,
        ENCODER_MOVEMENT_FINISHED = 2
    }
    // Delegates for callbacks
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void EncoderCallback(byte Port, EncoderEventType EventType, double Position, IntPtr UserData);

    private bool Disposed = false;
    private IPrinter PrinterHandle;
    private readonly object SyncRoot = new object();
    public PrinterController()
    {
        PrinterHandle = CreatePrinter();
        if (PrinterHandle.VirtualTable == IntPtr.Zero)
        {
            throw new InvalidOperationException("Failed to create printer instance");
        }
    }

    // ========== NEW FUNCTIONS ==========
    
    // Motor speed control
    public void SetMotorSpeed(byte Port, sbyte Speed)
    {
        lock (SyncRoot)
        {
            if (Disposed)
            {
                throw new ObjectDisposedException("PrinterController");
            }

            SafeCall(() => { PrinterSetMotorSpeed(PrinterHandle, Port, Speed); return true; }, false);
        }
    }

    // Command stream methods
    public void StartCommandStream(byte Port, MotorCommandExe[] Commands)
    {
        lock(SyncRoot)
        {
            if (Disposed)
            {
                throw new ObjectDisposedException("PrinterController");
            }
            SafeCall(() =>
            {
                CommandStream Stream = CreateCommandStream(Port, Commands);
                PrinterStartCommandStream(PrinterHandle, ref Stream);
                Marshal.FreeCoTaskMem(Stream.Commands); // Free allocated memory
                return true;
            }, false);
        }
    }

    public void UpdateCommandStream(byte port, MotorCommandExe[] commands)
    {
        lock (SyncRoot)
        {
            if (Disposed) throw new ObjectDisposedException("PrinterController");
            SafeCall(() =>
            {
                CommandStream stream = CreateCommandStream(port, commands);
                PrinterUpdateCommandStream(PrinterHandle, ref stream);
                Marshal.FreeCoTaskMem(stream.Commands); // Free allocated memory
                return true;
            }, false);
        }
    }

    public void StopCommandStream()
    {
        lock (SyncRoot)
        {
            if (Disposed) throw new ObjectDisposedException("PrinterController");
            SafeCall(() => { PrinterStopCommandStream(PrinterHandle); return true; }, false);
        }
    }

    // Speed profile execution
    public bool ExecuteSpeedProfile(byte port, SpeedProfilePoint[] points, int timeoutMs)
    {
        lock (SyncRoot)
        {
            if (Disposed) throw new ObjectDisposedException("PrinterController");
            return SafeCall(() =>
            {
                SpeedProfile profile = CreateSpeedProfile(port, points, timeoutMs);
                bool result = PrinterExecuteSpeedProfile(PrinterHandle, ref profile);
                Marshal.FreeCoTaskMem(profile.Points); // Free allocated memory
                return result;
            }, false);
        }
    }

    // Encoder event system
    public bool SubscribeToEncoderEvents(EncoderEvent[] events)
    {
        lock (SyncRoot)
        {
            if (Disposed) throw new ObjectDisposedException("PrinterController");
            return SafeCall(() =>
            {
                return PrinterSubscribeToEncoderEvents(PrinterHandle, events, events.Length);
            }, false);
        }
    }
    public bool UnsubscribeFromEncoderEvents(byte port)
    {
        lock (SyncRoot)
        {
            if (Disposed) throw new ObjectDisposedException("PrinterController");
            return SafeCall(() => PrinterUnsubscribeFromEncoderEvents(PrinterHandle, port), false);
        }
    }
    public bool WaitForEncoderEvent(byte port, EncoderEventType eventType,
        double targetPosition, double tolerance, int timeoutMs)
    {
        lock (SyncRoot)
        {
            if (Disposed) throw new ObjectDisposedException("PrinterController");
            return SafeCall(() => PrinterWaitForEncoderEvent(PrinterHandle, port, eventType,
                targetPosition, tolerance, timeoutMs), false);
        }
    }

    // Motor monitoring
    public bool IsMotorMoving(byte port)
    {
        lock (SyncRoot)
        {
            if (Disposed) throw new ObjectDisposedException("PrinterController");
            return SafeCall(() => PrinterIsMotorMoving(PrinterHandle, port), false);
        }
    }

    public double GetMotorPosition(byte port)
    {
        lock (SyncRoot)
        {
            if (Disposed) throw new ObjectDisposedException("PrinterController");
            return SafeCall(() => PrinterGetMotorPosition(PrinterHandle, port), 0.0);
        }
    }

    // ========== Helper methods ==========
    private CommandStream CreateCommandStream(byte Port, MotorCommandExe[] Commands)
    {
        int Size = Marshal.SizeOf(typeof(MotorCommandExe));
        IntPtr CommandsPtr = Marshal.AllocCoTaskMem(Size * Commands.Length);

        for (int i = 0; i < Commands.Length; i++)
        {
            IntPtr Pointer = new IntPtr(CommandsPtr.ToInt64() + i * Size);
            Marshal.StructureToPtr(Commands[i], Pointer, false);
        }

        return new CommandStream
        {
            Port = Port,
            Commands = CommandsPtr,
            Count = Commands.Length,
            Timestamp = (uint)DateTimeOffset.Now.ToUnixTimeSeconds()
        };
    }

    private SpeedProfile CreateSpeedProfile(byte Port, SpeedProfilePoint[] Points, int TimeoutMs)
    {
        int Size = Marshal.SizeOf(typeof(SpeedProfilePoint));
        IntPtr PointsPtr = Marshal.AllocCoTaskMem(Size * Points.Length);

        for (int i = 0; i < Points.Length; i++)
        {
            IntPtr Pointer = new IntPtr(PointsPtr.ToInt64() + i * Size);
            Marshal.StructureToPtr(Points[i], Pointer, false);
        }

        return new SpeedProfile
        {
            Port = Port,
            Points = PointsPtr,
            Count = Points.Length,
            TimeoutMs = TimeoutMs
        };
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

    public bool RunPrinterTest(string TestType)
    {
        lock (SyncRoot)
        {
            if (Disposed)
            {
                throw new ObjectDisposedException("PrinterController");                
            }

            return SafeCall(() => RunPrinterTest(PrinterHandle, TestType), false);
        }
    }
    public void PrintConnectionInfo() => SafeCall(() => { PrinterConnectionInfo(PrinterHandle); return true; }, false);
    public bool Connect() => SafeCall(() => PrinterConnect(PrinterHandle), false);
    public bool IsPrinterConnect() => SafeCall(() => IsConnected(PrinterHandle), false);
    public bool Disconnect() => SafeCall(() => PrinterDisconnect(PrinterHandle), false);

    public bool RequestBatteryLevel()
    {
        lock (SyncRoot)
        {
            if (Disposed) throw new ObjectDisposedException("PrinterController");
            Console.WriteLine($"[C#] RequestBatteryLevel called. PrinterHandle.VirtualTable: 0x{PrinterHandle.VirtualTable.ToInt64():X}");

            return SafeCall(() =>
            {
                Console.WriteLine($"[C#] Calling native PrinterRequestBatteryLevel...");
                bool result = PrinterRequestBatteryLevel(PrinterHandle);
                Console.WriteLine($"[C#] PrinterRequestBatteryLevel returned: {result}");
                return result;
            }, false);
        }
    }

    public byte GetBatteryLevel()
    {
        lock (SyncRoot)
        {
            if (Disposed) throw new ObjectDisposedException("PrinterController");
            Console.WriteLine($"[C#] GetBatteryLevel called. PrinterHandle.VirtualTable: 0x{PrinterHandle.VirtualTable.ToInt64():X}");

            return SafeCall(() =>
            {
                Console.WriteLine($"[C#] Calling native PrinterGetBatteryLevel...");
                byte level = PrinterGetBatteryLevel(PrinterHandle);
                Console.WriteLine($"[C#] PrinterGetBatteryLevel returned: {level}");
                return level;
            }, (byte)0);
        }
    }

    public bool IsBatteryLevelFresh(int maxAgeSeconds)
    {
        lock (SyncRoot)
        {
            if (Disposed) throw new ObjectDisposedException("PrinterController");
            Console.WriteLine($"[C#] IsBatteryLevelFresh called with maxAgeSeconds={maxAgeSeconds}");

            return SafeCall(() =>
            {
                Console.WriteLine($"[C#] Calling native PrinterIsBatteryLevelFresh...");
                bool isFresh = PrinterIsBatteryLevelFresh(PrinterHandle, maxAgeSeconds);
                Console.WriteLine($"[C#] PrinterIsBatteryLevelFresh returned: {isFresh}");
                return isFresh;
            }, false);
        }
    }

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

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterSetMotorSpeed(IPrinter printer, byte port, sbyte speed);

    // Command stream functions
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterStartCommandStream(IPrinter printer, ref CommandStream stream);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterUpdateCommandStream(IPrinter printer, ref CommandStream stream);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterStopCommandStream(IPrinter printer);

    // Speed profile functions
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterExecuteSpeedProfile(IPrinter printer, ref SpeedProfile profile);

    // Encoder event functions
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterSubscribeToEncoderEvents(IPrinter printer,
        [MarshalAs(UnmanagedType.LPArray)] EncoderEvent[] events, int count);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterUnsubscribeFromEncoderEvents(IPrinter printer, byte port);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterWaitForEncoderEvent(IPrinter printer, byte port,
        EncoderEventType eventType, double targetPosition, double tolerance, int timeoutMs);

    // Monitoring functions
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterIsMotorMoving(IPrinter printer, byte port);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern double PrinterGetMotorPosition(IPrinter printer, byte port);

    // Commands
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterSendCommand(IPrinter printer,
        [MarshalAs(UnmanagedType.LPArray)] byte[] data, int length);


    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool RunPrinterTest(IPrinter printer, string TestName);

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

    // Battery functions
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterRequestBatteryLevel(IPrinter printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern byte PrinterGetBatteryLevel(IPrinter printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterIsBatteryLevelFresh(IPrinter printer, int maxAgeSeconds);


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

    // Basic methods
    public bool Test(IPrinter printer) => TestCode(InterpreterHandle, printer);

    // CORRECTED method - correct order of parameters
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

            // CORRECT ORDER OF PARAMETERS: interpreter, filename, printer
            return ExecuteGcode(InterpreterHandle, fullPath, printer);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"C#: Exception in ExecuteFile: {ex}");
            return false;
        }
    }

    public bool ExecuteLine(string Line, IPrinter printer)
    {
        if (string.IsNullOrEmpty(Line))
        {
            Console.WriteLine("C#: ERROR - Line is null or empty");
            return false;
        }

        if (printer.VirtualTable == IntPtr.Zero)
        {
            Console.WriteLine("C#: ERROR - Printer VirtualTable is zero");
            return false;
        }

        try
        {
            return ExecuteLine(InterpreterHandle, Line, printer);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"C#: Exception in ExecuteLine: {ex}");
            throw;
        }
    }
    public void Pause() => PauseExecution(InterpreterHandle);
    public void Resume() => ResumeExecution(InterpreterHandle);
    public Status GetStatus() => (Status)GetStatus(InterpreterHandle);
    public double GetProgress() => GetProgress(InterpreterHandle);

    // String methods - NOW LIKE IN THE DRIVER!
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
    public bool ReadConfig(string filename) => ReadConfig(InterpreterHandle, filename);

    // Helper methods for C#
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

    // FIXED DllImport with correct order of parameters
    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool ExecuteGcode(IntPtr interpreter, string filename, IPrinter printer);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool ExecuteLine(IntPtr interpreter, string line, IPrinter printer);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PauseExecution(IntPtr interpreter);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void ResumeExecution(IntPtr interpreter);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int GetStatus(IntPtr interpreter);

    [DllImport("Interpreter.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern double GetProgress(IntPtr interpreter);

    // CHANGED: Now returns IntPtr instead of string
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
    private static extern bool ReadConfig(IntPtr interpreter, string filename);

    public void Dispose()
    {
        if (!Disposed && InterpreterHandle != IntPtr.Zero)
        {
            try
            {
                Console.WriteLine("Disposing GCodeInterpreter...");

                // We give time for proper completion
                for (int i = 0; i < 10; i++) // 10 attempts of 100 ms = 1 second
                {
                    if (!IsRunning)
                    {
                        break;
                    }

                    System.Threading.Thread.Sleep(100);
                }

                // Destroying the interpreter
                DestroyInterpreter(InterpreterHandle);
                InterpreterHandle = IntPtr.Zero;
                Disposed = true;

                Console.WriteLine("GCodeInterpreter disposed successfully");
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());

                // We'll mark it as disposed anyway.
                Disposed = true;
            }
        }
    }
}
