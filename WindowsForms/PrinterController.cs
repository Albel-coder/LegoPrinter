using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

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
    private static extern void PrinterRotateMotor(IPrinter printer, ref MotorCommand commands, int count);

    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterRotateMotor(IPrinter printer,
        [MarshalAs(UnmanagedType.LPArray)] MotorCommand[] commands, int count);

    // Commands
    [DllImport("LegoPrinterCore.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void PrinterSendCommand(IPrinter printer,
        [MarshalAs(UnmanagedType.LPArray)] byte[] data, int length);

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