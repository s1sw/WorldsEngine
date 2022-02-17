using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.ComponentMeta;

namespace WorldsEngine
{
    public class PhysicsActor : BuiltinComponent
    {
        internal static ComponentMetadata Metadata
        {
            get
            {
                if (cachedMetadata == null)
                    cachedMetadata = MetadataManager.FindNativeMetadata("Physics Actor")!;

                return cachedMetadata;
            }
        }

        private static ComponentMetadata? cachedMetadata;

        internal PhysicsActor(IntPtr regPtr, uint entityId) : base(regPtr, entityId) { }
    }
}
