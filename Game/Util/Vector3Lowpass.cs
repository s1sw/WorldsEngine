using WorldsEngine.Math;
namespace Game.Util;

public class Vector3Lowpass
{
    public float Q;
    private Vector3 _last;

    public Vector3 Value => _last;

    public Vector3Lowpass(float qVal)
    {
        Q = qVal;
    }

    public Vector3 Update(Vector3 val, float deltaTime)
    {
        _last = Vector3.Lerp(_last, val, deltaTime * Q);
        return _last;
    }
}