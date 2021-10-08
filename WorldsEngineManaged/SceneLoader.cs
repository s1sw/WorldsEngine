using System.Runtime.InteropServices;

namespace WorldsEngine
{
    public static class SceneLoader
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void sceneloader_loadScene(uint id);

        [DllImport(WorldsEngine.NativeModule)]
        private static extern uint sceneloader_getCurrentSceneID();

        public static void LoadScene(AssetID id) => sceneloader_loadScene(id.ID);
        public static AssetID CurrentSceneID => new AssetID(sceneloader_getCurrentSceneID());
    }
}
