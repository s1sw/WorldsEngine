using System.Runtime.InteropServices;
using WorldsEngine.Math;

namespace WorldsEngine
{
    public static partial class Physics
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool physics_raycast(Vector3 origin, Vector3 direction, float maxDist, uint excludeLayerMask, out RaycastHit hit);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern bool physics_overlapSphere(Vector3 origin, float radius, out uint overlappedEntity);

        [DllImport(WorldsEngine.NativeModule)]
        private static unsafe extern uint physics_overlapSphereMultiple(Vector3 origin, float radius, uint maxTouchCount, Entity* entityBuffer, uint excludeLayerMask);

        [DllImport(WorldsEngine.NativeModule)]
        [return: MarshalAs(UnmanagedType.I1)]
        private static unsafe extern bool physics_sweepSphere(Vector3 origin, float radius, Vector3 direction, float distance, out RaycastHit hit, uint excludeLayerMask);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void NativeCollisionDelegate(uint entityId, uint id, ref PhysicsContactInfo contactInfo);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern void physics_addEventHandler(NativeCollisionDelegate collisionDelegate);
    }
}
