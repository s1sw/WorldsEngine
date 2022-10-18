using System.Collections;
using System.Collections.Generic;

namespace WorldsEngine.ECS;

public readonly struct View<T1, T2>
{
    private readonly ComponentStorage<T1> _storage1;
    private readonly ComponentStorage<T2> _storage2;

    internal View(ComponentStorage<T1> storage1, ComponentStorage<T2> storage2)
    {
        _storage1 = storage1;
        _storage2 = storage2;
    }

    public ViewEnumerator<T1, T2> GetEnumerator()
    {
        return new ViewEnumerator<T1, T2>(_storage1, _storage2);
    }
}

public struct ViewEnumerator<T1, T2>
{
    private readonly ComponentStorage<T1> _storage1;
    private readonly ComponentStorage<T2> _storage2;
    private int _index = 0;
    
    internal ViewEnumerator(ComponentStorage<T1> storage1, ComponentStorage<T2> storage2)
    {
        _storage1 = storage1;
        _storage2 = storage2;
    }
    
    public bool MoveNext()
    {
        while (_index < _storage1.PackedEntities.Count && !_storage2.Contains(_storage1.PackedEntities[_index]))
        {
            _index++;
        }

        if (_index >= _storage1.PackedEntities.Count)
        {
            return false;
        }

        return true;
    }

    public void Reset()
    {
        _index = 0;
    }

    public (Entity, T1, T2) Current => 
        (
            _storage1.PackedEntities[_index], 
            _storage1.Components[_index],
            _storage2.Get(_storage1.PackedEntities[_index])
        );

    public void Dispose()
    {
    }
}
