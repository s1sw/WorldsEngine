using WorldsEngine;

namespace Game.Interaction;

[Component]
public class AudioOnGrip : Component, IStartListener
{
    public void Start()
    {
        var grabbable = Entity.GetComponent<Grabbable>();
        grabbable.OnGrabbed += (Grip g) => 
        {
            if (grabbable.AttachedHandFlags == AttachedHandFlags.Left || grabbable.AttachedHandFlags == AttachedHandFlags.Right)
                Entity.GetComponent<AudioSource>().Start();
        };

        grabbable.OnReleased += (Grip g) => 
        {
            if (grabbable.AttachedHandFlags == AttachedHandFlags.None)
                Entity.GetComponent<AudioSource>().Stop(StopMode.AllowFadeout);
        };

        if (Entity.TryGetComponent<Gun>(out Gun gun))
        {
            gun.OnFire += () =>
            {
                Entity.GetComponent<AudioSource>().Stop(StopMode.AllowFadeout);
                Entity.GetComponent<AudioSource>().Start();
            };
        }
    }
}