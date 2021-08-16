using System;

namespace WorldsEngine
{
    public struct Entity
    {
        public static Entity Null => new Entity(0x000FFFFF);

        public uint ID { get; private set; }
        public bool IsNull => Identifier == 0x000FFFFF;

        /// <summary>
        /// The 20-bit packed identifier value for this entity.
        /// </summary>
        internal uint Identifier
        {
            get { return ID & 0x000FFFFF; }
            set { ID |= value & 0x000FFFFF; }
        }

        /// <summary>
        /// The 12-bit packed version value for this entity.
        /// </summary>
        internal uint Version
        {
            get { return (ID & 0xFFF00000) >> 20; }
            set { ID = Identifier & (value << 20); }
        }

        internal Entity(uint id)
        {
            ID = id;
        }

        public static bool operator ==(Entity a, Entity b) => a.ID == b.ID;
        public static bool operator !=(Entity a, Entity b) => a.ID != b.ID;

        public override bool Equals(object obj)
        {
            return obj is Entity entity &&
                   ID == entity.ID;
        }

        public override int GetHashCode()
        {
            return HashCode.Combine(ID);
        }
    }
}
