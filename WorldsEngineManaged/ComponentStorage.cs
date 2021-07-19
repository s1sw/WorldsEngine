using System;
using System.Collections;
using System.Collections.Generic;
using System.Threading;

namespace WorldsEngine
{
    interface IComponentStorage
    {
    }

    public class ComponentStorage<T> : IComponentStorage, IEnumerable
    {
        public const int Size = 1000;
        public T[] components = new T[Size];
        public BitArray slotFree = new BitArray(Size, true);
        public static readonly int typeIndex;
        public static readonly Type type;
        public Entity[] packedEntities = new Entity[Size];
        public int[] sparseEntities = new int[Size];

        static ComponentStorage()
        {
            typeIndex = Interlocked.Increment(ref Registry.typeCounter);
            type = typeof(T);
        }

        internal ComponentStorage()
        {
            for (int i = 0; i < Size; i++)
            {
                sparseEntities[i] = -1;
                packedEntities[i] = Entity.Null;
            }
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
