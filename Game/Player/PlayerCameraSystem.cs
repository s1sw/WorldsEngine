using System;
using WorldsEngine;
using WorldsEngine.Math;
using WorldsEngine.Input;

namespace Game.Player;

[SystemUpdateOrder(10)]
public class PlayerCameraSystem : ISystem
{
    private static float lookX = 0.0f;
    private static float lookY = 0.0f;

    private Entity _listenerEntity;
    private static readonly Vector3 _toFloor = new(0.0f, -(1.85f * 0.5f), 0.0f);

    public void OnSceneStart()
    {
        lookX = 0.0f;
        lookY = 0.0f;

        if (VR.Enabled)
        {
            _listenerEntity = Registry.Create();
            Registry.SetName(_listenerEntity, "VR Audio Listener Override");

            Registry.AddComponent<AudioListenerOverride>(_listenerEntity);
        }
    }

    public static Vector3 GetCamPosForSimulation()
    {
        Transform bodyTransform = Registry.GetTransform(LocalPlayerSystem.PlayerBody);
        var bodyDpa = Registry.GetComponent<DynamicPhysicsActor>(LocalPlayerSystem.PlayerBody);

        if (!VR.Enabled)
        {
            return (bodyDpa.Pose.Position) + _toFloor + new Vector3(0.0f, 1.85f, 0.0f);
        }
        else
        {
            Vector3 hmdOffset = VRTransforms.HMDTransform.Position;
            hmdOffset.y = 0.0f;

            return bodyDpa.Pose.Position + bodyDpa.Velocity * Time.DeltaTime + LocalPlayerSystem.VirtualRotation * (-hmdOffset) + _toFloor;
        }
    }

    public static Vector3 WorldSpaceHeadPosition
    {
        get
        {
            if (VR.Enabled)
            {
                Transform bodyTransform = Registry.GetTransform(LocalPlayerSystem.PlayerBody);
                Vector3 hmdOffset = VRTransforms.HMDTransform.Position;
                hmdOffset *= Vector3.Up;

                return bodyTransform.Position + LocalPlayerSystem.VirtualRotation * (hmdOffset) + _toFloor;
            }
            else
            {
                Transform bodyTransform = Registry.GetTransform(LocalPlayerSystem.PlayerBody);
                return bodyTransform.Position + _toFloor + new Vector3(0.0f, 1.85f, 0.0f);
            }
        }
    }

    public static Quaternion WorldSpaceHeadRotation
    {
        get
        {
            if (!VR.Enabled)
            {
                Quaternion upDown = Quaternion.AngleAxis(lookY, new Vector3(1f, 0f, 0f));
                Quaternion leftRight = Quaternion.AngleAxis(-lookX, new Vector3(0f, 1f, 0f));

                return leftRight * upDown;
            }
            else
            {
                return LocalPlayerSystem.VirtualRotation * VRTransforms.HMDTransform.Rotation;
            }
        }
    }

    public static Transform WorldSpaceHeadTransform => new(WorldSpaceHeadPosition, WorldSpaceHeadRotation);

    // lol
    // Manually interpolate the camera position to add an extra frame of latency
    // so it fits with everything else that calculates its position a frame off
    private static Vector3 _lastLastBodyPos = Vector3.Zero;
    private static Vector3 _lastBodyPos = Vector3.Zero;

    public void OnSimulate()
    {
        _lastLastBodyPos = _lastBodyPos;
        _lastBodyPos = LocalPlayerSystem.PlayerBody.Transform.Position;
    }

    public void OnUpdate()
    {
        if (LocalPlayerSystem.PlayerBody.IsNull || FreecamSystem.Enabled) return;

        if (!VR.Enabled)
        {
            lookX += Mouse.PositionDelta.x * 0.005f;
            lookY += Mouse.PositionDelta.y * 0.005f;

            lookX += Controller.DeadzonedAxisValue(ControllerAxis.RightX) * 0.05f;
            lookY += Controller.DeadzonedAxisValue(ControllerAxis.RightY) * 0.05f;

            Quaternion upDown = Quaternion.AngleAxis(lookY, new Vector3(1f, 0f, 0f));
            Quaternion leftRight = Quaternion.AngleAxis(-lookX, new Vector3(0f, 1f, 0f));

            Quaternion cameraRotation = leftRight * upDown;


            if (Registry.Valid(LocalPlayerSystem.PlayerBody))
            {
                Camera.Main.Rotation = cameraRotation;
                Transform bodyTransform = Registry.GetTransform(LocalPlayerSystem.PlayerBody);
                Vector3 manualBpos = Vector3.Lerp(_lastLastBodyPos, _lastBodyPos, Time.InterpolationAlpha);
                Camera.Main.Position = bodyTransform.Position + _toFloor + new Vector3(0.0f, 1.85f, 0.0f);
            }
        }
        else
        {
            Vector3 hmdOffset = VRTransforms.HMDTransform.Position;
            hmdOffset.y = 0.0f;

            Transform bodyTransform = Registry.GetTransform(LocalPlayerSystem.PlayerBody);
            Camera.Main.Position = bodyTransform.Position + LocalPlayerSystem.VirtualRotation * (-hmdOffset) + _toFloor;
            Camera.Main.Rotation = LocalPlayerSystem.VirtualRotation;
            Transform listenerTransform = new();

            listenerTransform.Position = Camera.Main.Position + LocalPlayerSystem.VirtualRotation * (VRTransforms.HMDTransform.Position);
            listenerTransform.Rotation = LocalPlayerSystem.VirtualRotation * VRTransforms.HMDTransform.Rotation;
            listenerTransform.Scale = Vector3.One;

            Registry.SetTransform(_listenerEntity, listenerTransform);
        }
    }
}
