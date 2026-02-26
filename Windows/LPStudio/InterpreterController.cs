using System;
using System.Runtime.InteropServices;

/// <summary>
/// G-code interpreter states (must match C++ enum Status)
/// </summary>
public enum InterpreterStatus
{
    Idle = 0,
    CheckingCode = 1,
    Running = 2,
    Paused = 3,
    Completed = 4,
    Error = 5
}

/// <summary>
/// Control of the G-code interpreter (works in the same DLL as the printer driver)
/// </summary>
public class InterpreterController : IDisposable
{
    private IntPtr _interpreterHandle;
    private readonly IntPtr _printerHandle;
    private bool _disposed;
    private readonly object _syncRoot = new object();

    /// <summary>
    /// Creates an interpreter instance associated with an existing printer driver.
    /// </summary>
    /// <param name="printerHandle">Printer driver handle (from PrinterController.GetPrinterHandle())</param>
    public InterpreterController(IntPtr printerHandle)
    {
        if (printerHandle == IntPtr.Zero)
            throw new ArgumentException("Printer handle is invalid", nameof(printerHandle));

        _printerHandle = printerHandle;
        _interpreterHandle = CreateInterpreterNative(_printerHandle);

        if (_interpreterHandle == IntPtr.Zero)
            throw new InvalidOperationException("Failed to create interpreter instance");
    }

    /// <summary>
    /// Loads the axis configuration from a file.
    /// </summary>
    public bool ReadConfig(string filename)
    {
        lock (_syncRoot)
        {
            if (_disposed) throw new ObjectDisposedException(nameof(InterpreterController));
            return SafeCall(() => ReadConfigNative(_interpreterHandle, filename), false);
        }
    }

    /// <summary>
    /// Starts execution of G-code from a file (asynchronously, in a separate thread).
    /// </summary>
    public bool ExecuteFile(string filename)
    {
        lock (_syncRoot)
        {
            if (_disposed) throw new ObjectDisposedException(nameof(InterpreterController));
            return SafeCall(() => ExecuteGCodeNative(_interpreterHandle, filename), false);
        }
    }

    /// <summary>
    /// Executes one line of G-code (synchronously).
    /// </summary>
    public bool ExecuteLine(string line)
    {
        lock (_syncRoot)
        {
            if (_disposed) throw new ObjectDisposedException(nameof(InterpreterController));
            return SafeCall(() => ExecuteLineNative(_interpreterHandle, line), false);
        }
    }

    /// <summary>
    /// Pauses execution (if a file is running).
    /// </summary>
    public void Pause()
    {
        lock (_syncRoot)
        {
            if (_disposed) return;
            PauseExecutionNative(_interpreterHandle);
        }
    }

    /// <summary>
    /// Resumes execution after a pause.
    /// </summary>
    public void Resume()
    {
        lock (_syncRoot)
        {
            if (_disposed) return;
            ResumeExecutionNative(_interpreterHandle);
        }
    }

    /// <summary>
    /// Stops execution immediately (via a separate C++ API).
    /// </summary>
    public void Stop()
    {
        lock (_syncRoot)
        {
            if (_disposed) return;
            StopExecutionNative(_interpreterHandle);
        }
    }

    /// <summary>
    /// Current status of the interpreter.
    /// </summary>
    public InterpreterStatus Status
    {
        get
        {
            if (_disposed) return InterpreterStatus.Error;

            int status = SafeCall(() => GetStatusNative(_interpreterHandle), (int)InterpreterStatus.Error);

            // Check that the value is within the valid range of the enum
            if (Enum.IsDefined(typeof(InterpreterStatus), status))
                return (InterpreterStatus)status;

            return InterpreterStatus.Error;
        }
    }

    /// <summary>
    /// File execution progress (from 0 to 100).
    /// </summary>
    public double Progress
    {
        get
        {
            if (_disposed) return 0.0;
            return SafeCall(() => GetProgressNative(_interpreterHandle), 0.0);
        }
    }

    /// <summary>
    /// Last interpreter error.
    /// </summary>
    public string LastError
    {
        get
        {
            if (_disposed) return string.Empty;

            return SafeCall(() =>
            {
                IntPtr ptr = GetLastInterpreterErrorNative(_interpreterHandle);
                return ptr != IntPtr.Zero ? Marshal.PtrToStringAnsi(ptr) : string.Empty;
            }, string.Empty);
        }
    }

    // ================= SAFE CALL =================

    private T SafeCall<T>(Func<T> function, T defaultValue)
    {
        if (_disposed)
            return defaultValue;

        try
        {
            return function();
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Interpreter SafeCall exception: {ex}");
            return defaultValue;
        }
    }

    // ================= IDISPOSABLE =================
    public void Dispose()
    {
        if (_disposed) return;

        lock (_syncRoot)
        {
            if (_disposed) return;

            try
            {
                if (_interpreterHandle != IntPtr.Zero)
                {
                    DestroyInterpreterNative(_interpreterHandle);
                    _interpreterHandle = IntPtr.Zero;
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error disposing interpreter: {ex}");
            }

            _disposed = true;
            GC.SuppressFinalize(this);
        }
    }

    ~InterpreterController()
    {
        Dispose();
    }

    // ================= DLL IMPORTS =================

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr CreateInterpreterNative(IntPtr printer);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void DestroyInterpreterNative(IntPtr handle);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool ExecuteGCodeNative(IntPtr handle, string filename);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool ExecuteLineNative(IntPtr handle, string line);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PauseExecutionNative(IntPtr handle);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void ResumeExecutionNative(IntPtr handle);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void StopExecutionNative(IntPtr handle);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int GetStatusNative(IntPtr handle);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern double GetProgressNative(IntPtr handle);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr GetLastInterpreterErrorNative(IntPtr handle);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool ReadConfigNative(IntPtr handle, string filename);
}
