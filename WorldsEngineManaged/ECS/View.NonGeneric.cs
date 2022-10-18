using System.Collections;
using System.Collections.Generic;

namespace WorldsEngine.ECS;

public readonly struct View
{
    private readonly IComponentStorage _storage;
    private readonly ViewEnumerator _enumerator;

    internal View(IComponentStorage storage)
    {
        if (storage == null)
            throw new System.ArgumentNullException(nameof(storage));
        _storage = storage;
        _enumerator = new ViewEnumerator(storage);
    }

    public ViewEnumerator GetEnumerator()
    {
        return _enumerator;
    }
}

public struct ViewEnumerator : IEnumerator<Entity>
{
    private readonly IComponentStorage _storage;
    private int _index = 0;
    
    internal ViewEnumerator(IComponentStorage storage)
    {
        _storage = storage;
    }
    
    public bool MoveNext()
    {
        if (++_index >= _storage.Count)
        {
            return false;
        }
        
        return true;
    }

    public void Reset()
    {
        _index = 0;
    }

    public Entity Current => _storage.GetPackedEntity(_index);

    object IEnumerator.Current => Current;

    public void Dispose()
    {
    }
}
