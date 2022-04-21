using System;
using System.Runtime.InteropServices;
namespace WorldsEngine.NativeInterop;

[StructLayout(LayoutKind.Explicit)]
struct SlibString
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

    //[FieldOffset(9)]
    //[MarshalAs(UnmanagedType.ByValArray, SizeConst = 14)]
    //public byte[] SmallChars;

    //public string Convert()
    //{
    //    if (SSO)
    //    {
    //        return System.Text.Encoding.UTF8.GetString(SmallChars.AsSpan(0, SmallLength));
    //    }
    //    else
    //    {
    //        return Marshal.PtrToStringUTF8(Data, (int)Length);
    //    }
    //}
}