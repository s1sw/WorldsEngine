using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine.Util
{
    public static class DevMaterials
    {
        public static AssetID Blue => AssetDB.PathToId("Materials/DevTextures/dev_blue.json");
        public static AssetID Green => AssetDB.PathToId("Materials/DevTextures/dev_green.json");
        public static AssetID Metal => AssetDB.PathToId("Materials/DevTextures/dev_metal.json");
        public static AssetID Red => AssetDB.PathToId("Materials/DevTextures/dev_red.json");
        public static AssetID Orange => AssetDB.PathToId("Materials/dev.json");
        public static AssetID ReflectionTest => AssetDB.PathToId("Materials/refltest.json");
    }
}
