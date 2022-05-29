using System;
using System.Collections;
using System.Collections.Generic;
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
        object GetBoxed(Entity entity);
        void SetBoxed(Entity entity, object val);
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

    public class ComponentStorage<T> : IComponentStorage, IEnumerable
    {
        public Type Type { get { return type; } }
        public readonly List<T> components = new();
        public static readonly int typeIndex;
        public static readonly Type type;
        public readonly List<Entity> packedEntities = new();
        public bool IsThinking { get; private set; }
        public bool IsUpdateable { get; private set; }
        public int Count => packedEntities.Count;

        private readonly SparseStorage _sparseStorage = new();

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

        public ComponentStorage()
        {
            IsThinking = Type.IsAssignableTo(typeof(IThinkingComponent));
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

        public void Set(Entity entity, T component)
        {
            int index = packedEntities.Count;

            _sparseStorage[entity] = index;

            // Update the sparse set
            packedEntities.Add(entity);
            components.Add(component);
        }

        public void SetBoxed(Entity entity, object component)
        {
            int index = packedEntities.Count;

            _sparseStorage[entity] = index;

            // Update the sparse set
            packedEntities.Add(entity);
            components.Add((T)component);
        }

        public void Remove(Entity entity)
        {
            if (!Contains(entity))
                throw new ArgumentException($"Trying to remove component {typeof(T).Name} that isn't there");

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

        public Entity GetFirst()
        {
            return packedEntities[0];
        }

        public void RunSimulate()
        {
            foreach (Entity entity in packedEntities)
            {
                ((IThinkingComponent)components[GetIndexOf(entity)]!).Think();
            }
        }

        public void RunUpdate()
        {
            foreach (Entity entity in packedEntities)
            {
                ((IUpdateableComponent)components[GetIndexOf(entity)]!).Update();
            }
        }
    }
}
