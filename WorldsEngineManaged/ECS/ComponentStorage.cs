using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
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
        List<Entity>.Enumerator GetEnumerator();
    }

    struct SerializedComponentStorage
    {
        public string FullTypeName;
        public List<Entity> Packed;
        public SparseStorage SparseStorage;
        public List<SerializedObject> Components;
    }

    internal class ComponentTypeLookup
    {
        public static Dictionary<string, int> typeIndices = new();
        public static Dictionary<string, SerializedComponentStorage> serializedComponents = new();
    }

    class SparseStorage
    {
        const int PageSize = 500;
        private List<int[]> _pages = new();

        public SparseStorage()
        {
            // Start off with 1 page
            ResizePages(0);
        }

        private void ResizePages(int requiredIndex)
        {
            for (int i = _pages.Count; i <= requiredIndex; i++)
            {
                _pages.Add(new int[PageSize]);

                for (int j = 0; j < PageSize; j++)
                {
                    _pages[i][j] = -1;
                }
            }
        }

        public int this[Entity entity]
        {
            get
            {
                int pageIndex = (int)(entity.Identifier / PageSize);
                int valIndex = (int)(entity.Identifier % PageSize);

                return _pages[pageIndex][valIndex];
            }

            set
            {
                int pageIndex = (int)(entity.Identifier / PageSize);
                int valIndex = (int)(entity.Identifier % PageSize);

                if (_pages.Count <= pageIndex)
                    ResizePages(pageIndex);

                _pages[pageIndex][valIndex] = value;
            }
        }

        public bool Contains(Entity entity)
        {
            int pageIndex = (int)(entity.Identifier / PageSize);
            int valIndex = (int)(entity.Identifier % PageSize);

            return pageIndex < _pages.Count && _pages[pageIndex][valIndex] != -1;
        }
    }

    public class ComponentStorage<T> : IComponentStorage, IEnumerable
    {
        public Type Type { get { return type; } }
        public List<T> components = new();
        public static readonly int typeIndex;
        public static readonly Type type;
        public List<Entity> packedEntities = new();
        public bool IsThinking { get; private set; }

        private SparseStorage _sparseStorage = new();

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

            if (!hotload || !ComponentTypeLookup.serializedComponents.ContainsKey(type.FullName!))
                return;

            var serializedStorage = ComponentTypeLookup.serializedComponents[type.FullName!];

            _sparseStorage = serializedStorage.SparseStorage;
            packedEntities = serializedStorage.Packed;
            components = serializedStorage.Components.Select(x => (T)HotloadSerialization.Deserialize(x)!).ToList();

            ComponentTypeLookup.serializedComponents.Remove(type.FullName!);
        }

        public bool Contains(Entity entity)
        {
            return _sparseStorage.Contains(entity);
        }

        public T Get(Entity entity)
        {
            if (!Contains(entity))
            {
                throw new ArgumentException("Entity doesn't have that component");
            }

            return components[GetIndexOf(entity)];
        }

        public object GetBoxed(Entity entity)
        {
            if (!Contains(entity))
            {
                throw new ArgumentException("Entity doesn't have that component");
            }

            return components[GetIndexOf(entity)]!;
        }

        internal void Set(Entity entity, T component)
        {
            Logger.Log($"Set entity {entity.ID}");

            int index = packedEntities.Count;

            _sparseStorage[entity] = index;

            // Update the sparse set
            packedEntities.Add(entity);
            components.Add(component);
        }

        public void SetBoxed(Entity entity, object component)
        {
            int index = packedEntities.Count;
            // Put the component in and mark the slot as used.

            _sparseStorage[entity] = index;

            // Update the sparse set
            packedEntities.Add(entity);
            components.Add((T)component);
        }

        public void Remove(Entity entity)
        {
            if (!Contains(entity))
                throw new ArgumentException("Trying to remove a component that isn't there");

            int index = GetIndexOf(entity);
            int lastIndex = packedEntities.Count - 1;

            // To remove an entity, we want to swap the entity we're removing with the one at the end of the packed list.
            Entity replacementEntity = packedEntities[lastIndex];
            T replacementComponent = components[lastIndex];

            _sparseStorage[replacementEntity] = index;

            packedEntities[index] = replacementEntity;
            components[index] = replacementComponent;

            _sparseStorage[entity] = -1;

            
            packedEntities.RemoveAt(lastIndex);
            components.RemoveAt(lastIndex);
        }

        public void SerializeForHotload()
        {
            var serializedStorage = new SerializedComponentStorage()
            {
                Packed = packedEntities,
                SparseStorage = _sparseStorage,
                Components = components.Select(x => HotloadSerialization.Serialize(x)).ToList(),
                FullTypeName = Type.FullName!
            };

            ComponentTypeLookup.serializedComponents.Add(type.FullName!, serializedStorage);
        }

        private int GetIndexOf(Entity entity)
        {
            return _sparseStorage[entity];
        }

        IEnumerator IEnumerable.GetEnumerator()
        {
            return GetEnumerator();
        }

        public List<Entity>.Enumerator GetEnumerator()
        {
            return packedEntities.GetEnumerator();
        }

        public void UpdateIfThinking()
        {
            foreach (Entity entity in packedEntities)
            {
                ((IThinkingComponent)components[GetIndexOf(entity)]!).Think(entity);
            }
        }

        private void ExpandPackedList(int newSize)
        {
            if (newSize <= packedEntities.Count) return;

            packedEntities.AddRange(Enumerable.Repeat(Entity.Null, newSize - packedEntities.Count));
        }
    }
}
