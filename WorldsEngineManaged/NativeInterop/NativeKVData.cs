using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine.NativeInterop
{
    [StructLayout(LayoutKind.Sequential)]
    struct NativeKVObject
    {
        public ulong NumElements;
        public IntPtr ElementsPtrPtr;
    }

    [StructLayout(LayoutKind.Explicit)]
    unsafe struct NativeKVValue
    {
        [FieldOffset(0)]
        public KVType Type;
        [FieldOffset(4)]
        public double DoubleValue;
        [FieldOffset(4)]
        public long LongValue;
        [FieldOffset(4)]
        public IntPtr StringPtrValue;
        [FieldOffset(4)]
        public NativeKVObject* ObjectValue;

        public string StringValue => Marshal.PtrToStringUTF8(StringPtrValue)!;
    }

    internal enum KVType : int
    {
        Null,
        Double,
        Int64,
        String,
        Object
    }

    internal class NativeKVData
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static unsafe extern NativeKVValue* nativekv_getValue(IntPtr kvObject, IntPtr utf8Key);
    }
}
