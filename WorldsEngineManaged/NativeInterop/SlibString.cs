using System;
using System.Runtime.InteropServices;
namespace WorldsEngine.NativeInterop;

[StructLayout(LayoutKind.Explicit, Size = 24)]
public struct SlibString
{
    [FieldOffset(0)]
    [MarshalAs(UnmanagedType.I1)]
    public bool SSO;

    [FieldOffset(8)]
    public IntPtr Data;

    [FieldOffset(16)]
    public ulong Length;

    [FieldOffset(8)]
    public byte SmallLength;

    public unsafe string Convert()
    {
        if (SSO)
        {
            string str;
            fixed (byte* ptr = &SmallLength)
            {
                byte* stringStart = ptr + 1;
                str = Marshal.PtrToStringUTF8((IntPtr)stringStart, SmallLength);
            }
            return str;
        }
        else
        {
            return Marshal.PtrToStringUTF8(Data, (int)Length);
        }
    }
}