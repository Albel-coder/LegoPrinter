using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Ports;
using System.Net.NetworkInformation;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using static PrinterController;

// Class for managing a printer
public class PrinterController : IDisposable
{
    // Data structures for interfacing with C++ DLLs  

    public enum HubMode
    {
        Unknown = 0,
        LegoOfficial = 1,
        Bootloader = 2,
        PybricksRuntime = 3,
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1, CharSet = CharSet.Ansi)]
    public struct PrinterDeviceInfo
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
        public string Address;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
        public string Name;
        public int Rssi;
        public int IsLegoHub;
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
    private IntPtr PrinterHandle;
    private readonly object SyncRoot = new object();
    public PrinterController()
    {
        PrinterHandle = CreatePrinter();
        if (PrinterHandle == IntPtr.Zero)
        {
            throw new InvalidOperationException("Failed to create printer instance");
        }
    }

    // ========== NEW FUNCTIONS ==========

    public List<PrinterDeviceInfo> Scan(int timeoutSeconds, bool legoOnly = true)
    {
        const int maxDevices = 32;
        PrinterDeviceInfo[] buffer = new PrinterDeviceInfo[maxDevices];
        int count = SafeCall(() => PrinterScan(PrinterHandle, timeoutSeconds, legoOnly ? 1 : 0, buffer, maxDevices), 0);
        var result = new List<PrinterDeviceInfo>();
        for (int i = 0; i < count && i < maxDevices; i++)
        {
            result.Add(buffer[i]);
        }
        return result;
    }

    public bool ConnectAuto(int timeoutMs, bool legoOnly = true) =>
        SafeCall(() => PrinterConnectAuto(PrinterHandle, timeoutMs, legoOnly ? 1 : 0), false);

    public bool Connect(string address) => 
        SafeCall(() => PrinterConnect(PrinterHandle, address), false);

    public bool ReconnectLast() =>
        SafeCall(() => PrinterReconnectLast(PrinterHandle), false);

    public bool Disconnect() => 
        SafeCall(() => PrinterDisconnect(PrinterHandle), false);

    public bool IsPrinterConnect() 
        => SafeCall(() => IsConnected(PrinterHandle), false);

    public string GetConnectedAddress()
    {
        var stringBuilder = new StringBuilder(64);
        int result = SafeCall(() => PrinterGetConnectedAddress(PrinterHandle, stringBuilder, stringBuilder.Capacity), 0);
        return result != 0 ? stringBuilder.ToString() : null;   
    }

    public List<PrinterDeviceInfo> GetRecentHubs()
    {
        int count = SafeCall(() => PrinterGetRecentHubCount(PrinterHandle), 0);
        var list = new List<PrinterDeviceInfo>();
        for (int i = 0; i < count; i++)
        {
            if (PrinterGetRecentHub(PrinterHandle, i, out PrinterDeviceInfo hub) != 0)
            {
                list.Add(hub);
            }
        }
        return list;
    }

    public HubMode DetectHubMode(string address)
    {
        int rawResult = SafeCall(() => PrinterDetectHubMode(PrinterHandle, address), -1);
        Console.WriteLine($"[C#] DetectHubMode raw result = {rawResult}");
        return (HubMode)rawResult;
    }

    public bool ProbeRuntime(string address, int timeoutMs) =>
        SafeCall(() => PrinterProbeRuntime(PrinterHandle, address, timeoutMs), false);

    public bool FlashFirmware(string firmwareZipPath, string address = "") =>
        SafeCall(() => PrinterFlashFirmware(PrinterHandle, firmwareZipPath, address), false);

    public bool UploadProgram(string scriptPath, string address = "") =>
        SafeCall(() => PrinterUploadProgram(PrinterHandle, scriptPath, address), false);

    public bool StartUserProgram() =>
        SafeCall(() => PrinterStartUserProgram(PrinterHandle), false);

    public bool StopUserProgram() =>
        SafeCall(() => PrinterStopUserProgram(PrinterHandle), false);

    public bool ConnectRuntime(string address) =>
        SafeCall(() => PrinterConnectRuntime(PrinterHandle, address), false);

    public bool SendRuntime(byte[] data) =>
        SafeCall(() => PrinterSendRuntime(PrinterHandle, data, data.Length), false);

    public bool RuntimeRotateMotor(byte port, int speed, int angle, bool hold) =>
        SafeCall(() => PrinterRuntimeRotateMotor(PrinterHandle, port, speed, angle, hold));

    public bool RuntimeRing() =>
        SafeCall(() => PrinterRuntimePing(PrinterHandle));













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
        
    public bool RequestBatteryLevel()
    {
        lock (SyncRoot)
        {
            if (Disposed) throw new ObjectDisposedException("PrinterController");
            
            return SafeCall(() =>
            {
                bool result = PrinterRequestBatteryLevel(PrinterHandle);
                return result;
            }, false);
        }
    }

    public byte GetBatteryLevel()
    {
        lock (SyncRoot)
        {
            if (Disposed) throw new ObjectDisposedException("PrinterController");            

            return SafeCall(() =>
            {
                byte level = PrinterGetBatteryLevel(PrinterHandle);
                return level;
            }, (byte)0);
        }
    }

    public bool IsBatteryLevelFresh(int maxAgeSeconds)
    {
        lock (SyncRoot)
        {
            if (Disposed) throw new ObjectDisposedException("PrinterController");

            return SafeCall(() =>
            {
                bool isFresh = PrinterIsBatteryLevelFresh(PrinterHandle, maxAgeSeconds);
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
    private static extern IntPtr CreatePrinter();

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void DestroyPrinter(IntPtr printer);

    // Connection functions
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int PrinterScan(IntPtr printer, int timeoutSeconds, int legoOnly, 
        [Out] PrinterDeviceInfo[] outDevices, int maxDevices);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterConnectAuto(IntPtr printer, int timeoutMs, int legoOnly);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterConnect(IntPtr printer, string address);
       
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterReconnectLast(IntPtr printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterDisconnect(IntPtr printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool IsConnected(IntPtr printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int PrinterGetConnectedAddress(IntPtr printer, StringBuilder outAddress, int capacity);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int PrinterGetRecentHubCount(IntPtr printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int PrinterGetRecentHub(IntPtr printer, int index, out PrinterDeviceInfo outHub);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int PrinterDetectHubMode(IntPtr printer, string address);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterProbeRuntime(IntPtr printer, string address, int timeoutMs);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterFlashFirmware(IntPtr printer, string firmwareBootloaderPath, string address);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterUploadProgram(IntPtr printer, string scriptPath, string address);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterStartUserProgram(IntPtr printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterStopUserProgram(IntPtr printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterConnectRuntime(IntPtr printer, string address);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterDisconnectRuntime(IntPtr printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterSendRuntime(IntPtr printer, [MarshalAs(UnmanagedType.LPArray)] byte[] data, int length);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterSendMotorCommands(IntPtr printer,
        [MarshalAs(UnmanagedType.LPArray)] MotorCommand[] commands, int count);




    // Functions for controlling motors

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterRotateMotor(IntPtr printer,
        [MarshalAs(UnmanagedType.LPArray)] MotorCommand[] commands, int count);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterSetMotorSpeed(IntPtr printer, byte port, sbyte speed);

    // Command stream functions
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterStartCommandStream(IntPtr printer, ref CommandStream stream);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterUpdateCommandStream(IntPtr printer, ref CommandStream stream);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterStopCommandStream(IntPtr printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterRuntimeRotateMotor(IntPtr printer, byte port, int speed, int angle, bool hold);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterRuntimePing(IntPtr printer);


    // Speed profile functions
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterExecuteSpeedProfile(IntPtr printer, ref SpeedProfile profile);

    // Encoder event functions
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterSubscribeToEncoderEvents(IntPtr printer,
        [MarshalAs(UnmanagedType.LPArray)] EncoderEvent[] events, int count);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterUnsubscribeFromEncoderEvents(IntPtr printer, byte port);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterWaitForEncoderEvent(IntPtr printer, byte port,
        EncoderEventType eventType, double targetPosition, double tolerance, int timeoutMs);

    // Monitoring functions
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterIsMotorMoving(IntPtr printer, byte port);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern double PrinterGetMotorPosition(IntPtr printer, byte port);

    // Commands
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterSendCommand(IntPtr printer,
        [MarshalAs(UnmanagedType.LPArray)] byte[] data, int length);


    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool RunPrinterTest(IntPtr printer, string TestName);

    // System Logging
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int GetLogCount(IntPtr printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr GetLogEntry(IntPtr printer, int index);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void ClearLog(IntPtr printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr GetLastErrorMessage(IntPtr printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterConnectionInfo(IntPtr printer);

    // Battery functions
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterRequestBatteryLevel(IntPtr printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern byte PrinterGetBatteryLevel(IntPtr printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterIsBatteryLevelFresh(IntPtr printer, int maxAgeSeconds);

    public IntPtr GetPrinterHandle()
    {
        return PrinterHandle;
    }

    // To properly release resources
    public void Dispose()
    {
        if (!Disposed)
        {
            try
            {
                Console.WriteLine("Disposing PrinterController...");

                System.Threading.Thread.Sleep(100);

                DestroyPrinter(PrinterHandle);

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
