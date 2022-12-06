using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

namespace WorldsEngine
{
    public enum CombineMode : uint
    {
        Average = 0,
        Min = 1,
        Multiply = 2,
        Max = 3
    }

    public class PhysicsMaterial : IDisposable
    {
        [DllImport(Engine.NativeModule)]
        private static extern IntPtr physicsmaterial_new(float staticFriction, float dynamicFriction, float restitution);

        [DllImport(Engine.NativeModule)]
        private static extern void physicsmaterial_acquireReference(IntPtr material);

        [DllImport(Engine.NativeModule)]
        private static extern void physicsmaterial_release(IntPtr material);

        [DllImport(Engine.NativeModule)]
        private static extern float physicsmaterial_getStaticFriction(IntPtr material);

        [DllImport(Engine.NativeModule)]
        private static extern float physicsmaterial_getDynamicFriction(IntPtr material);

        [DllImport(Engine.NativeModule)]
        private static extern float physicsmaterial_getRestitution(IntPtr material);

        [DllImport(Engine.NativeModule)]
        private static extern void physicsmaterial_setStaticFriction(IntPtr material, float val);

        [DllImport(Engine.NativeModule)]
        private static extern void physicsmaterial_setDynamicFriction(IntPtr material, float val);

        [DllImport(Engine.NativeModule)]
        private static extern void physicsmaterial_setRestitution(IntPtr material, float val);

        [DllImport(Engine.NativeModule)]
        private static extern void physicsmaterial_setFrictionCombineMode(IntPtr material, CombineMode mode);

        [DllImport(Engine.NativeModule)]
        private static extern CombineMode physicsmaterial_getFrictionCombineMode(IntPtr material);

        public float StaticFriction
        {
            get => physicsmaterial_getStaticFriction(_nativeHandle);
            set => physicsmaterial_setStaticFriction(_nativeHandle, value);
        }

        public float DynamicFriction
        {
            get => physicsmaterial_getDynamicFriction(_nativeHandle);
            set => physicsmaterial_setDynamicFriction(_nativeHandle, value);
        }

        public float Restitution
        {
            get => physicsmaterial_getRestitution(_nativeHandle);
            set => physicsmaterial_setRestitution(_nativeHandle, value);
        }

        public CombineMode FrictionCombineMode
        {
            get => physicsmaterial_getFrictionCombineMode(NativeHandle);
            set => physicsmaterial_setFrictionCombineMode(NativeHandle, value);
        }

        internal IntPtr NativeHandle => _nativeHandle;

        private readonly IntPtr _nativeHandle;
        private bool _disposedValue;

        public PhysicsMaterial(float staticFriction, float dynamicFriction, float restitution)
        {
            _nativeHandle = physicsmaterial_new(staticFriction, dynamicFriction, restitution);
        }

        internal PhysicsMaterial(IntPtr nativePtr)
        {
            _nativeHandle = nativePtr;
            physicsmaterial_acquireReference(_nativeHandle);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposedValue)
            {
                physicsmaterial_release(_nativeHandle);
                _disposedValue = true;
            }
        }

        ~PhysicsMaterial()
        {
            Dispose(disposing: false);
        }

        public void Dispose()
        {
            // Do not change this code. Put cleanup code in 'Dispose(bool disposing)' method
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }
    }
}
