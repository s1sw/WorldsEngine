using System.Collections;
using System.Collections.Generic;

namespace WorldsEngine.ECS;

public readonly struct View<T>
{
    private readonly ComponentStorage<T> _storage;
    private readonly ViewEnumerator<T> _enumerator;

    public int Count => _storage.Count;
    public Entity First => _storage.PackedEntities[0];

    internal View(ComponentStorage<T> storage)
    {
        _storage = storage;
        _enumerator = new(storage);
    }

    public ViewEnumerator<T> GetEnumerator()
    {
        return new ViewEnumerator<T>(_storage);
    }
}

public struct ViewEnumerator<T>
{
    private readonly ComponentStorage<T> _storage;
    private int _index = 0;
    
    internal ViewEnumerator(ComponentStorage<T> storage)
    {
        _storage = storage;
    }
    
    public bool MoveNext()
    {
        if (++_index >= _storage.Components.Count)
        {
            return false;
        }
        
        return true;
    }

    public void Reset()
    {
        _index = 0;
    }

    public (Entity, T) Current => (_storage.PackedEntities[_index], _storage.Components[_index]);

    public void Dispose()
    {
    }
}
