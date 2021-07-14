namespace WorldsEngine
{
    public struct AssetID {
        public uint ID { get; private set; }

        internal AssetID(uint id) {
            ID = id;
        }
    };
}
