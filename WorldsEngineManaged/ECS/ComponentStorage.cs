using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Reflection;
using System.Threading;

namespace WorldsEngine.ECS
{
    public interface IComponentStorage
    {
        Type Type { get; }
        bool IsThinking { get; }
        bool IsUpdateable { get; }
        int Count { get; }
        Entity GetPackedEntity(int index);
        object GetAsObject(Entity entity);
        void SetFromObject(Entity entity, object val);
        bool Contains(Entity entity);
        void Remove(Entity entity);
        void RunSimulate();
        void RunUpdate();
        List<Entity>.Enumerator GetEnumerator();
    }

    internal class ComponentTypeLookup
    {
        public static Dictionary<string, int> typeIndices = new();
    }

    // We use the fact that static members are unique to each generic instantiation
    // to get this working.
    [SuppressMessage("ReSharper", "StaticMemberInGenericType")]
    public class ComponentStorage<T> : IComponentStorage, IEnumerable
    {
        public Type Type => ComponentType;
        public readonly List<T> Components = new();
        public static readonly int TypeIndex;
        public static readonly Type ComponentType;
        public readonly List<Entity> PackedEntities = new();
        public bool IsThinking { get; private set; }
        public bool IsUpdateable { get; private set; }
        public int Count => PackedEntities.Count;

        private readonly SparseStorage _sparseStorage = new();

        static ComponentStorage()
        {
            ComponentType = typeof(T);
            if (ComponentTypeLookup.typeIndices.ContainsKey(ComponentType.FullName!))
            {
                TypeIndex = ComponentTypeLookup.typeIndices[ComponentType.FullName!];
            }
            else
            {
               TypeIndex = Interlocked.Increment(ref Registry.typeCounter);
               ComponentTypeLookup.typeIndices.Add(ComponentType.FullName!, TypeIndex);
            }
        }

        public ComponentStorage()
        {
            IsThinking = Type.IsAssignableTo(typeof(ISimulatedComponent));
            IsUpdateable = Type.IsAssignableTo(typeof(IUpdateableComponent));
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

            return Components[GetIndexOf(entity)];
        }

        public Entity GetPackedEntity(int index) => PackedEntities[index];

        public object GetAsObject(Entity entity)
        {
            if (!Contains(entity))
            {
                throw new ArgumentException("Entity doesn't have that component");
            }

            return Components[GetIndexOf(entity)]!;
        }

        public void Set(Entity entity, T component)
        {
            int index = PackedEntities.Count;

            _sparseStorage[entity] = index;

            // Update the sparse set
            PackedEntities.Add(entity);
            Components.Add(component);
        }

        public void SetFromObject(Entity entity, object component)
        {
            int index = PackedEntities.Count;

            _sparseStorage[entity] = index;

            // Update the sparse set
            PackedEntities.Add(entity);
            Components.Add((T)component);
        }

        public void Remove(Entity entity)
        {
            if (!Contains(entity))
                throw new ArgumentException($"Trying to remove component {typeof(T).Name} that isn't there");

            int index = GetIndexOf(entity);
            int lastIndex = PackedEntities.Count - 1;

            // To remove an entity, we want to swap the entity we're removing with the one at the end of the packed list.
            Entity replacementEntity = PackedEntities[lastIndex];
            T replacementComponent = Components[lastIndex];

            _sparseStorage[replacementEntity] = index;

            PackedEntities[index] = replacementEntity;
            Components[index] = replacementComponent;

            _sparseStorage[entity] = -1;
            
            PackedEntities.RemoveAt(lastIndex);
            Components.RemoveAt(lastIndex);
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
            return PackedEntities.GetEnumerator();
        }

        public Entity GetFirst()
        {
            return PackedEntities[0];
        }

        public void RunSimulate()
        {
            foreach (Entity entity in PackedEntities)
            {
                System.Diagnostics.Debug.Assert(Contains(entity));
                ((ISimulatedComponent)Components[GetIndexOf(entity)]!).Simulate();
            }
        }

        public void RunUpdate()
        {
            foreach (Entity entity in PackedEntities)
            {
                System.Diagnostics.Debug.Assert(Contains(entity));
                ((IUpdateableComponent)Components[GetIndexOf(entity)]!).Update();
            }
        }
    }
}
