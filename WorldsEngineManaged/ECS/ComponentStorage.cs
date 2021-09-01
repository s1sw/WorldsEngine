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
        bool IsThinking { get; }
        void SerializeForHotload();
        object GetBoxed(Entity entity);
        void SetBoxed(Entity entity, object val);
        bool Contains(Entity entity);
        void Remove(Entity entity);
        void UpdateIfThinking();
    }

    struct SerializedComponentStorage
    {
        public string FullTypeName;
        public Entity[] Packed;
        public int[] Sparse;
        public SerializedObject[] Components;
    }

    internal class ComponentTypeLookup
    {
        public static Dictionary<string, int> typeIndices = new Dictionary<string, int>();
        public static Dictionary<string, SerializedComponentStorage> serializedComponents = new Dictionary<string, SerializedComponentStorage>();
    }

    public class ComponentStorage<T> : IComponentStorage, IEnumerable
    {
        public Type Type { get { return type; } }
        public const int Size = 500;
        public T[] components = new T[Size];
        public BitArray slotFree = new BitArray(Size, true);
        public static readonly int typeIndex;
        public static readonly Type type;
        public Entity[] packedEntities = new Entity[Size];
        public int[] sparseEntities = new int[Size];
        public bool IsThinking { get; private set; }

        static ComponentStorage()
        {
            type = typeof(T);
            if (ComponentTypeLookup.typeIndices.ContainsKey(type.FullName!))
            {
                typeIndex = ComponentTypeLookup.typeIndices[type.FullName!];
            }
            else
            {
                typeIndex = Interlocked.Increment(ref Registry.typeCounter);
                ComponentTypeLookup.typeIndices.Add(type.FullName!, typeIndex);
            }
        }

        internal ComponentStorage(bool hotload = false)
        {
            IsThinking = Type.IsAssignableTo(typeof(IThinkingComponent));

            if (!hotload)
            {
                for (int i = 0; i < Size; i++)
                {
                    sparseEntities[i] = -1;
                    packedEntities[i] = Entity.Null;
                }

                return;
            }

            if (!ComponentTypeLookup.serializedComponents.ContainsKey(type.FullName!))
            {
                for (int i = 0; i < Size; i++)
                {
                    sparseEntities[i] = -1;
                    packedEntities[i] = Entity.Null;
                }

                return;
            }

            var serializedStorage = ComponentTypeLookup.serializedComponents[type.FullName!];

            for (int i = 0; i < Size; i++)
            {
                sparseEntities[i] = serializedStorage.Sparse[i];
                packedEntities[i] = serializedStorage.Packed[i];
                components[i] = (T)HotloadSerialization.Deserialize(serializedStorage.Components[i]);
            }

            ComponentTypeLookup.serializedComponents.Remove(type.FullName!);
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
            return entity.Identifier < sparseEntities.Length && GetSlotFor(entity) != -1;
        }

        public T Get(Entity entity)
        {
            if (!Contains(entity))
            {
                throw new ArgumentException("Entity doesn't have that component");
            }

            return components[GetSlotFor(entity)];
        }

        public object GetBoxed(Entity entity)
        {
            if (!Contains(entity))
            {
                throw new ArgumentException("Entity doesn't have that component");
            }

            return components[GetSlotFor(entity)]!;
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

        public void SetBoxed(Entity entity, object component)
        {
            // First find a free index to put the new component in.
            int freeIndex = GetFreeIndex();

            // Put the component in and mark the slot as used.
            components[freeIndex] = (T)component;
            slotFree[freeIndex] = false;

            // Update the sparse set.
            packedEntities[freeIndex] = entity;
            sparseEntities[entity.Identifier] = freeIndex;
        }

        public void Remove(Entity entity)
        {
            if (!Contains(entity))
                throw new ArgumentException("Trying to remove a component that isn't there");

            int index = GetSlotFor(entity);

            slotFree[index] = true;
            sparseEntities[entity.Identifier] = -1;
            packedEntities[index] = Entity.Null;
        }

        public void SerializeForHotload()
        {
            var serializedStorage = new SerializedComponentStorage()
            {
                Packed = packedEntities,
                Sparse = sparseEntities,
                Components = new SerializedObject[Size],
                FullTypeName = Type.FullName!
            };

            for (int i = 0; i < Size; i++)
            {
                serializedStorage.Components[i] = HotloadSerialization.Serialize(components[i]!);
            }

            ComponentTypeLookup.serializedComponents.Add(type.FullName!, serializedStorage);
        }

        private int GetSlotFor(Entity entity)
        {
            return sparseEntities[entity.Identifier];
        }

        IEnumerator IEnumerable.GetEnumerator()
        {
            return GetEnumerator();
        }

        public ComponentStorageEnum GetEnumerator()
        {
            return new ComponentStorageEnum(packedEntities);
        }

        public void UpdateIfThinking()
        {
            foreach (Entity entity in this)
            {
                ((IThinkingComponent)components[GetSlotFor(entity)]!).Think(entity);
            }
        }
    }

    public class ComponentStorageEnum : IEnumerator
    {
        readonly Entity[] packed;
        int index = -1;

        public ComponentStorageEnum(Entity[] packed)
        {
            this.packed = packed;
        }

        public bool MoveNext()
        {
            index++;
            return !packed[index].IsNull;
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
