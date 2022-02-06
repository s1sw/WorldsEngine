using WorldsEngine;
using Game.Interaction;
using WorldsEngine.Math;

namespace Game;

[Component]
class GrapplingHook : IThinkingComponent, IStartListener
{
    private bool _grappling = false;
    private bool _grapplingDynamic = false;
    private Entity _grapplingEntity = Entity.Null;
    private Vector3 _targetPos = Vector3.Zero;

    public void Start(Entity entity)
    {
        var grabbable = Registry.GetComponent<Grabbable>(entity);
        grabbable.TriggerPressed += Grabbable_TriggerPressed;
        grabbable.TriggerReleased += Grabbable_TriggerReleased;
    }

    private void Grabbable_TriggerPressed(Entity entity)
    {
        var transform = Registry.GetTransform(entity);
        // Raycast and find a hit point to pull the player to
        if (Physics.Raycast(transform.Position + transform.Forward, transform.Forward, out var hit))
        {
            _grapplingDynamic = Registry.HasComponent<DynamicPhysicsActor>(hit.HitEntity);
            _grapplingEntity = hit.HitEntity;
            _grappling = true;

            if (!_grapplingDynamic)
                _targetPos = hit.WorldHitPos;
            else
                _targetPos = Registry.GetTransform(hit.HitEntity).InverseTransformPoint(hit.WorldHitPos);
        }
    }

    private void Grabbable_TriggerReleased(Entity entity)
    {
        _grappling = false;
        _grapplingDynamic = false;
    }

    public void Think(Entity entity)
    {
        if (_grappling)
        {
            var pos = Registry.GetTransform(entity).Position;
            float forceMagnitude = 27.5f * 80f;

            if (_grapplingDynamic)
                forceMagnitude /= 2f;

            Vector3 targetPosActual = _grapplingDynamic ? Registry.GetTransform(_grapplingEntity).TransformPoint(_targetPos) : _targetPos;
            Vector3 forceDir = (targetPosActual - pos).Normalized;
            LocalPlayerSystem.AddForceToRig(forceDir * forceMagnitude);

            if (_grapplingDynamic)
            {
                if (!Registry.Valid(_grapplingEntity))
                {
                    _grapplingDynamic = false;
                    _grapplingEntity = Entity.Null;
                }
                else
                {
                    var dpa = Registry.GetComponent<DynamicPhysicsActor>(_grapplingEntity);
                    dpa.AddForce(-forceDir * forceMagnitude);
                }
            }
        }
    }
}
