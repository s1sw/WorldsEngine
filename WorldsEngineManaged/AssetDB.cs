using System.Text;
using System.Runtime.InteropServices;
using System;
using WorldsEngine.PhysFS;

namespace WorldsEngine
{
    public static class AssetDB
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void assetDB_idToPath(uint id, out uint length, StringBuilder? str);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint assetDB_pathToId(string path);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern byte assetDB_exists(uint id);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern IntPtr assetDB_openAssetRead(uint id);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern IntPtr assetDB_openAssetWrite(uint id);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint assetDB_createAsset(string path);


        public static string IdToPath(AssetID id)
        {
            assetDB_idToPath(id.ID, out uint length, null);

            StringBuilder sb = new StringBuilder((int)length);
            assetDB_idToPath(id.ID, out _, sb);
            
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

        public static AssetID CreateAsset(string path)
        {
            return new AssetID(assetDB_createAsset(path));
        }

        internal static PhysFSFileStream OpenAssetRead(AssetID id)
        {
            return new PhysFSFileStream(assetDB_openAssetRead(id.ID), false);
        }

        internal static PhysFSFileStream OpenAssetWrite(AssetID id)
        {
            return new PhysFSFileStream(assetDB_openAssetWrite(id.ID), true);
        }
    }
}
