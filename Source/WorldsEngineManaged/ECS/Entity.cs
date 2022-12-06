using System;
using System.Text.Json.Serialization;

namespace WorldsEngine.ECS;

public struct Entity
{
    public static Entity Null => new(0x000FFFFF);

    public uint ID { get; private set; }

    [JsonIgnore] public bool IsNull => Identifier == 0x000FFFFF;

    [JsonIgnore] public bool IsValid => Registry.Valid(this);

    /// <summary>
    /// The 20-bit packed identifier value for this entity.
    /// </summary>
    [JsonIgnore]
    internal uint Identifier
    {
        get { return ID & 0x000FFFFF; }
        set { ID |= value & 0x000FFFFF; }
    }

    /// <summary>
    /// The 12-bit packed version value for this entity.
    /// </summary>
    [JsonIgnore]
    internal uint Version
    {
        get { return (ID & 0xFFF00000) >> 20; }
        set { ID = Identifier & (value << 20); }
    }

    public Entity(uint id)
    {
        ID = id;
    }

    public static bool operator ==(Entity a, Entity b) => a.ID == b.ID;
    public static bool operator !=(Entity a, Entity b) => a.ID != b.ID;

    public override bool Equals(object? obj)
    {
        return obj != null &&
               obj is Entity entity &&
               ID == entity.ID;
    }

    public override int GetHashCode()
    {
        return HashCode.Combine(ID);
    }

    public T GetComponent<T>() => Registry.GetComponent<T>(this);
    public bool HasComponent<T>() => Registry.HasComponent<T>(this);
    public bool TryGetComponent<T>(out T comp) => Registry.TryGetComponent<T>(this, out comp);

    [JsonIgnore]
    public Transform Transform
    {
        get => Registry.GetTransform(this);
        set => Registry.SetTransform(this, value);
    }
}
