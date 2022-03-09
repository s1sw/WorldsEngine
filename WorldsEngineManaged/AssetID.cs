namespace WorldsEngine
{
    public struct AssetID {
        public uint ID { get; private set; }
        public static AssetID InvalidID => new AssetID(~0u);

        internal AssetID(uint id) {
            ID = id;
        }
    };
}
