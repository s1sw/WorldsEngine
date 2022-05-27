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
        [DllImport(Engine.NativeModule)]
        private static extern void audio_playOneShot(uint clipId, ref Vector3 location, [MarshalAs(UnmanagedType.I1)] bool spatialise, float volume, uint channel);

        [DllImport(Engine.NativeModule)]
        private static extern void audio_playOneShotEvent(string eventPath, ref Vector3 location, float volume);

        [DllImport(Engine.NativeModule)]
        private static extern void audio_playOneShotAttachedEvent(string eventPath, ref Vector3 location, uint entity, float volume);

        [DllImport(Engine.NativeModule)]
        private static extern void audio_loadBank(string bankPath);

        [DllImport(Engine.NativeModule)]
        private static extern void audio_stopEverything(IntPtr regPtr);

        [DllImport(Engine.NativeModule)]
        private static extern void audio_updateAudioScene(IntPtr regPtr);

        [Obsolete("Use PlayOneShotEvent")]
        public static void PlayOneShot(AssetID clipId, float volume, MixerChannel channel = MixerChannel.SFX)
        {
            Vector3 loc = Vector3.Zero;
            audio_playOneShot(clipId.ID, ref loc, false, volume, (uint)channel);
        }

        [Obsolete("Use PlayOneShotEvent")]
        public static void PlayOneShot(AssetID clipId, Vector3 location, float volume, MixerChannel channel = MixerChannel.SFX)
        {
            audio_playOneShot(clipId.ID, ref location, true, volume, (uint)channel);
        }

        public static void PlayOneShotEvent(string eventPath, Vector3 location, float volume = 1f)
            => audio_playOneShotEvent(eventPath, ref location, volume);

        public static void PlayOneShotEvent(string eventPath, float volume = 1f)
        {
            Vector3 location = Vector3.Zero;
            audio_playOneShotEvent(eventPath, ref location, volume);
        }

        public static void PlayOneShotAttachedEvent(string eventPath, Vector3 location, Entity entity, float volume = 1f)
            => audio_playOneShotAttachedEvent(eventPath, ref location, entity.ID, volume);

        public static void LoadBank(string bankPath) => audio_loadBank(bankPath);

        public static void StopEverything() => audio_stopEverything(Registry.NativePtr);
        public static void UpdateAudioScene() => audio_updateAudioScene(Registry.NativePtr);
    }
}
