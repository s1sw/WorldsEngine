using System;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using System.Threading;

namespace WorldsEngine
{
    interface IComponentStorage
    {
        Type Type { get; }
        void Serialize();
    }

    struct SerializedComponentStorage
    {
        public Entity[] packed;
        public int[] sparse;
        public SerializedType[] components;
    }

    internal class ComponentTypeLookup
    {
        public static Dictionary<string, int> typeIndices = new Dictionary<string, int>();
        public static Dictionary<string, SerializedComponentStorage> serializedComponents = new Dictionary<string, SerializedComponentStorage>();
    }

    public class ComponentStorage<T> : IComponentStorage, IEnumerable
    {
        public Type Type { get { return type; } }
        public const int Size = 1000;
        public T[] components = new T[Size];
        public BitArray slotFree = new BitArray(Size, true);
        public static readonly int typeIndex;
        public static readonly Type type;
        public Entity[] packedEntities = new Entity[Size];
        public int[] sparseEntities = new int[Size];

        static ComponentStorage()
        {
            type = typeof(T);
            if (ComponentTypeLookup.typeIndices.ContainsKey(type.FullName))
            {
                typeIndex = ComponentTypeLookup.typeIndices[type.FullName];
            }
            else
            {
                typeIndex = Interlocked.Increment(ref Registry.typeCounter);
                ComponentTypeLookup.typeIndices.Add(type.FullName, typeIndex);
            }
        }

        internal ComponentStorage()
        {
            for (int i = 0; i < Size; i++)
            {
                sparseEntities[i] = -1;
                packedEntities[i] = Entity.Null;
            }
        }

        internal ComponentStorage(Assembly gameAssembly)
        {
            // Assume that this is a hotload

            if (!ComponentTypeLookup.serializedComponents.ContainsKey(type.FullName))
            {
                for (int i = 0; i < Size; i++)
                {
                    sparseEntities[i] = -1;
                    packedEntities[i] = Entity.Null;
                }

                return;
            }

            var serializedStorage = ComponentTypeLookup.serializedComponents[type.FullName];

            for (int i = 0; i < Size; i++)
            {
                sparseEntities[i] = serializedStorage.sparse[i];
                packedEntities[i] = serializedStorage.packed[i];
                components[i] = (T)HotloadSerialization.Deserialize(gameAssembly, serializedStorage.components[i], null);
            }

            ComponentTypeLookup.serializedComponents.Remove(type.FullName);
        }

        internal int GetFreeIndex()
        {
            for (int i = 0; i < slotFree.Length; i++)
            {
                if (slotFree[i])
                    return i;
            }

            throw new ApplicationException("Out of component slots");
        }

        public bool Contains(Entity entity)
        {
            return GetSlotFor(entity) != -1;
        }

        public ref T Get(Entity entity)
        {
            if (!Contains(entity))
            {
                throw new ArgumentException("Entity doesn't have that component");
            }

            return ref components[GetSlotFor(entity)];
        }

        internal void Set(Entity entity, T component)
        {
            Logger.Log($"Set entity {entity.ID}");
            // First find a free index to put the new component in.
            int freeIndex = GetFreeIndex();

            // Put the component in and mark the slot as used.
            components[freeIndex] = component;
            slotFree[freeIndex] = false;

            // Update the sparse set.
            packedEntities[freeIndex] = entity;
            sparseEntities[entity.Identifier] = freeIndex;
        }

        internal void Remove(Entity entity)
        {
            if (!Contains(entity))
                throw new ArgumentException("Trying to remove a component that isn't there");

            int index = GetSlotFor(entity);

            slotFree[index] = true;
            sparseEntities[entity.Identifier] = -1;
        }

        public void Serialize()
        {
            var serializedStorage = new SerializedComponentStorage();

            for (int i = 0; i < Size; i++)
            {
                serializedStorage.packed[i] = packedEntities[i];
                serializedStorage.sparse[i] = sparseEntities[i];
                serializedStorage.components[i] = HotloadSerialization.Serialize(components[i]);
            }

            ComponentTypeLookup.serializedComponents.Add(type.FullName, serializedStorage);
        }

        private int GetSlotFor(Entity entity)
        {
            return sparseEntities[entity.Identifier];
        }

        IEnumerator IEnumerable.GetEnumerator()
        {
            return (IEnumerator)GetEnumerator();
        }

        public ComponentStorageEnum GetEnumerator()
        {
            return new ComponentStorageEnum(sparseEntities, packedEntities);
        }
    }

    public class ComponentStorageEnum : IEnumerator
    {
        int[] sparse;
        Entity[] packed;
        int index = -1;

        public ComponentStorageEnum(int[] sparse, Entity[] packed)
        {
            this.sparse = sparse;
            this.packed = packed;
        }

        public bool MoveNext()
        {
            index++;
            return (!packed[index].IsNull);
        }

        public void Reset()
        {
            index = -1;
        }

        object IEnumerator.Current
        {
            get
            {
                return Current;
            }
        }

        public Entity Current
        {
            get
            {
                return packed[index];
            }
        }
    }
}
