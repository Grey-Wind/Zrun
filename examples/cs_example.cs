using System;
using System.Runtime.InteropServices;

public class Zrun
{
    [DllImport("zrun.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr zrun_create();
    
    [DllImport("zrun.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void zrun_destroy(IntPtr instance);
    
    [DllImport("zrun.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern ZrunResult zrun_execute_sync(IntPtr instance, string command, 
                                                     ZrunShellType shellType, int timeoutMs);
    
    [DllImport("zrun.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int zrun_execute_async(IntPtr instance, string command, 
                                               ZrunShellType shellType, int timeoutMs,
                                               OutputCallback callback, IntPtr userData);
    
    // 其他函数声明...
    
    private IntPtr instance;
    
    public Zrun()
    {
        instance = zrun_create();
    }
    
    ~Zrun()
    {
        zrun_destroy(instance);
    }
    
    public ZrunResult ExecuteSync(string command, ZrunShellType shellType = ZrunShellType.PowerShell, 
                                int timeoutMs = 30000)
    {
        return zrun_execute_sync(instance, command, shellType, timeoutMs);
    }
    
    // 其他方法...
}

public enum ZrunShellType
{
    CMD,
    PowerShell,
    Bash
}

[StructLayout(LayoutKind.Sequential)]
public struct ZrunResult
{
    public int exitCode;
    public IntPtr output;
    public IntPtr error;
    public long executionTime;
    public int timedOut;
    
    public string Output => Marshal.PtrToStringAnsi(output);
    public string Error => Marshal.PtrToStringAnsi(error);
}

public delegate void OutputCallback(IntPtr output, int isError, IntPtr userData);