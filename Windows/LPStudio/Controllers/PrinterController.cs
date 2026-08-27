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

    public bool FlashFirmware(string firmwareZipPath, string address = "") =>
        SafeCall(() => PrinterFlashFirmware(PrinterHandle, firmwareZipPath, address), false);

    public bool UploadProgram(string scriptPath, string address = "") =>
        SafeCall(() => PrinterUploadProgram(PrinterHandle, scriptPath, address), false);

    public bool ConnectRuntime(string address) =>
        SafeCall(() => PrinterConnectRuntime(PrinterHandle, address), false);

    public int GetLogCount() => SafeCall(() => GetLogCount(PrinterHandle), 0);

    public string GetLogEntry(int Index)
    {
        return SafeCall(() =>
        {
            IntPtr logEntryPtr = GetLogEntry(PrinterHandle, Index);
            return logEntryPtr != IntPtr.Zero ? Marshal.PtrToStringAnsi(logEntryPtr) : string.Empty;
        }, string.Empty);
    }

    public void ClearLog() => SafeCall(() => { ClearLog(PrinterHandle); return true; }, false);

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
    private static extern bool PrinterFlashFirmware(IntPtr printer, string firmwareBootloaderPath, string address);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterUploadProgram(IntPtr printer, string scriptPath, string address);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool PrinterConnectRuntime(IntPtr printer, string address);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool RunPrinterTest(IntPtr printer, string TestName);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int GetLogCount(IntPtr printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr GetLogEntry(IntPtr printer, int index);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void ClearLog(IntPtr printer);

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

public sealed class MotionCompilerController : IDisposable
{
    private IntPtr compilerHandle;
    private bool disposed;

    public MotionCompilerController()
    {
        compilerHandle = CreateMotionCompiler();

        if (compilerHandle == IntPtr.Zero)
        {
            throw new InvalidOperationException("Failed to create MotionCompiler instance.");
        }
    }

    public bool GenerateGCode(string inputPath, string outputPath, bool useSkeleton = false)
    {
        ThrowIfDisposed();

        if (string.IsNullOrWhiteSpace(inputPath))
        {
            throw new ArgumentException("Input path cannot be empty.", nameof(inputPath));
        }

        if (string.IsNullOrWhiteSpace(outputPath))
        {
            throw new ArgumentException("Output path cannot be empty.", nameof(outputPath));
        }

        return CompileImageProfiles(compilerHandle, inputPath, outputPath, useSkeleton);
    }

    private void ThrowIfDisposed()
    {
        if (disposed)
        {
            throw new ObjectDisposedException(nameof(MotionCompilerController));
        }
    }

    [DllImport("MotionCompiler.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr CreateMotionCompiler();

    [DllImport("MotionCompiler.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool CompileImageProfiles(IntPtr compiler, string inputFilename, string outputFilename, [MarshalAs(UnmanagedType.I1)] bool useSkeleton);

    [DllImport("MotionCompiler.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void DestroyMotionCompiler(IntPtr compiler);

    public void Dispose()
    {
        if (disposed)
        {
            return;
        }

        if (compilerHandle != IntPtr.Zero)
        {
            DestroyMotionCompiler(compilerHandle);
            compilerHandle = IntPtr.Zero;
        }

        disposed = true;

        GC.SuppressFinalize(this);
    }

    ~MotionCompilerController()
    {
        Dispose();
    }
}