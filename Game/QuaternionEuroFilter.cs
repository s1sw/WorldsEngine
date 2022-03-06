using System;
using ImGuiNET;
using WorldsEngine.Math;

namespace Game;

class QuaternionLowPassFilter
{
    public Quaternion PreviousResult => _prev;
    private Quaternion _prev = Quaternion.Identity;
    private bool _firstRun = true;

    public Quaternion Filter(Quaternion q, float alpha)
    {
        if (_firstRun)
        {
            _prev = q;
            _firstRun = false;
            return q;
        }

        Quaternion result = Quaternion.Slerp(_prev, q, alpha);
        _prev = result;

        return result;
    }
}

class QuaternionEuroFilter
{
    public float MinCutoff = 1.0f;
    public float DerivativeCutoff = 1.0f;
    public float Beta = 0.5f;

    private QuaternionLowPassFilter _dxFilter = new();
    private QuaternionLowPassFilter _lpFilter = new();
    private bool _firstRun = true;
    private Quaternion _dx = Quaternion.Identity;

    private float CalcAlpha(float dt, float cutoff)
    {
        float tau = 1f / (2f * MathF.PI * cutoff);
        return 1f / (1f + tau / dt);
    }

    public Quaternion Filter(Quaternion q, float dt)
    {
        if (_firstRun)
        {
            _firstRun = false;
        }
        else
        {
            _dx = q * _lpFilter.PreviousResult.Inverse;

            float rate = 1f / dt;
            _dx.x *= rate;
            _dx.y *= rate;
            _dx.z *= rate;
            _dx.w = _dx.w * rate + (1f - rate);
            _dx = _dx.Normalized;
        }

        float edx = 2f * MathF.Acos(_dxFilter.Filter(_dx, CalcAlpha(dt, DerivativeCutoff)).w);
        float cutoff = MinCutoff + Beta * edx;
        ImGui.Text($"edx: {edx}");
        ImGui.Text($"dx: {_dx}");

        return _lpFilter.Filter(q, CalcAlpha(dt, cutoff));
    }
}
