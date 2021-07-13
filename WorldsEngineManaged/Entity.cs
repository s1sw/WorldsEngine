namespace WorldsEngine
{
    public struct Entity
    {
        public uint ID { get; private set; }
        
        internal Entity(uint id)
        {
            this.ID = id;
        }
    }
}
