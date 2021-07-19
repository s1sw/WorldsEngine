using System.Text;
using System.Runtime.InteropServices;

namespace WorldsEngine
{
    public static class AssetDB
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void assetDB_idToPath(uint id, out uint length, StringBuilder str);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint assetDB_pathToId(string path);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern byte assetDB_exists(uint id);


        public static string IdToPath(AssetID id)
        {
            uint length;
            assetDB_idToPath(id.ID, out length, null);

            StringBuilder sb = new StringBuilder((int)length);
            assetDB_idToPath(id.ID, out length, sb);
            
            return sb.ToString();
        }

        public static AssetID PathToId(string path)
        {
            return new AssetID(assetDB_pathToId(path));
        }

        public static bool Exists(AssetID id)
        {
            return assetDB_exists(id.ID) == 1;
        }
    }
}
