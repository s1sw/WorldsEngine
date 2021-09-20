using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.ComponentMeta;

namespace WorldsEngine
{
    public class AudioListenerOverride : BuiltinComponent
    {
        internal static ComponentMetadata Metadata
        {
            get
            {
                if (cachedMetadata == null)
                    cachedMetadata = MetadataManager.FindNativeMetadata("FMOD Audio Source")!;

                return cachedMetadata;
            }
        }

        private static ComponentMetadata? cachedMetadata;

        internal AudioListenerOverride(IntPtr regPtr, uint entityId) : base(regPtr, entityId)
        {
        }
    }
}
