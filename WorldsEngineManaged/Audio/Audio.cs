using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine.Math;

namespace WorldsEngine.Audio
{
    public enum MixerChannel : uint
    {
        Music,
        SFX
    }

    public static class Audio
    {
        [DllImport(WorldsEngine.NativeModule)]
        private static extern void audio_playOneShot(uint clipId, ref Vector3 location, [MarshalAs(UnmanagedType.I1)] bool spatialise, float volume, uint channel);

        public static void PlayOneShot(AssetID clipId, float volume, MixerChannel channel = MixerChannel.SFX)
        {
            Vector3 loc = Vector3.Zero;
            audio_playOneShot(clipId.ID, ref loc, false, volume, (uint)channel);
        }

        public static void PlayOneShot(AssetID clipId, Vector3 location, float volume, MixerChannel channel = MixerChannel.SFX)
        {
            audio_playOneShot(clipId.ID, ref location, true, volume, (uint)channel);
        }
    }
}
