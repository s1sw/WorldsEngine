using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine.PhysFS
{
    internal class PhysFSNative
    {
        [DllImport(Engine.NativeModule)]
        public static extern IntPtr PHYSFS_openRead(string filename);

        [DllImport(Engine.NativeModule)]
        public static extern IntPtr PHYSFS_openWrite(string filename);

        [DllImport(Engine.NativeModule)]
        public static extern int PHYSFS_close(IntPtr fileHandle);

        [DllImport(Engine.NativeModule)]
        public static extern long PHYSFS_readBytes(IntPtr fileHandle, IntPtr buffer, ulong len);

        [DllImport(Engine.NativeModule)]
        public static extern long PHYSFS_writeBytes(IntPtr fileHandle, IntPtr buffer, ulong len);

        [DllImport(Engine.NativeModule)]
        public static extern int PHYSFS_flush(IntPtr fileHandle);

        [DllImport(Engine.NativeModule)]
        public static extern long PHYSFS_fileLength(IntPtr fileHandle);

        [DllImport(Engine.NativeModule)]
        public static extern long PHYSFS_tell(IntPtr fileHandle);

        [DllImport(Engine.NativeModule)]
        public static extern int PHYSFS_seek(IntPtr fileHandle, ulong position);
    }

    internal static class PhysFS
    {
        public static PhysFSFileStream OpenRead(string path)
        {
            return new PhysFSFileStream(PhysFSNative.PHYSFS_openRead(path), false);
        }

        public static PhysFSFileStream OpenWrite(string path)
        {
            return new PhysFSFileStream(PhysFSNative.PHYSFS_openWrite(path), true);
        }
    }

    internal class PhysFSFileStream : Stream
    {
        private readonly bool _isWriteStream = false;
        private readonly IntPtr _physfsHandle;
        private bool _disposed = false;

        public override bool CanRead => !_isWriteStream;
        public override bool CanSeek => true;
        public override bool CanWrite => _isWriteStream;

        public override long Length => PhysFSNative.PHYSFS_fileLength(_physfsHandle);

        public override long Position { get => PhysFSNative.PHYSFS_tell(_physfsHandle); set => PhysFSNative.PHYSFS_seek(_physfsHandle, (ulong)value); }

        internal PhysFSFileStream(IntPtr physfsHandle, bool isWriteStream)
        {
            _physfsHandle = physfsHandle;
            _isWriteStream = isWriteStream;
        }

        public override void Flush()
        {
            if (PhysFSNative.PHYSFS_flush(_physfsHandle) == 0)
                throw new IOException("Failed to flush file");
        }

        public override int Read(byte[] buffer, int offset, int count)
        {
            if (offset + count > buffer.Length) throw new ArgumentException("The sum of offset and count is larger than the buffer length.");

            byte[] tempBuffer = new byte[count];

            GCHandle handle = GCHandle.Alloc(tempBuffer, GCHandleType.Pinned);
            long readBytes = PhysFSNative.PHYSFS_readBytes(_physfsHandle, handle.AddrOfPinnedObject(), (ulong)count);
            handle.Free();

            tempBuffer.CopyTo(buffer, offset);

            return (int)readBytes;
        }

        public override long Seek(long offset, SeekOrigin origin)
        {
            ulong actualOffset = origin switch
            {
                SeekOrigin.Current => (ulong)(Position + offset),
                SeekOrigin.End => (ulong)(Position - offset),
                _ => (ulong)offset
            };

            if (PhysFSNative.PHYSFS_seek(_physfsHandle, actualOffset) != 0)
                throw new IOException("Failed to seek");

            return Position;
        }

        public override void SetLength(long value)
        {
            throw new NotSupportedException();
        }

        public override void Write(byte[] buffer, int offset, int count)
        {
            byte[] tempBuffer = new byte[count];

            Array.Copy(buffer, offset, tempBuffer, 0, count);

            GCHandle handle = GCHandle.Alloc(tempBuffer, GCHandleType.Pinned);
            PhysFSNative.PHYSFS_writeBytes(_physfsHandle, handle.AddrOfPinnedObject(), (ulong)count);
            handle.Free();
        }

        protected override void Dispose(bool disposing)
        {
            if (_disposed) return;
            _disposed = true;

            PhysFSNative.PHYSFS_close(_physfsHandle);

            base.Dispose(disposing);
        }
    }
}
