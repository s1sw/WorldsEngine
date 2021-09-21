using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;

namespace Game
{
    internal static class HapticManager
    {
        private static VRAction _leftHandHaptics;
        private static VRAction _rightHandHaptics;

        private static bool _initialized = false;

        public static void Initialize()
        {
            if (!VR.Enabled) return;

            _leftHandHaptics = new VRAction("/actions/main/out/VibrationLeft");
            _rightHandHaptics = new VRAction("/actions/main/out/VibrationRight");

            _initialized = true;
        }

        public static void Trigger(AttachedHandFlags handFlags, float timeFromNow, float duration, float frequency, float amplitude)
        {
            if (!VR.Enabled) return;

            if (!_initialized) Initialize();

            if (handFlags.HasFlag(AttachedHandFlags.Left))
            {
                _leftHandHaptics.TriggerHaptics(timeFromNow, duration, frequency, amplitude);
            }

            if (handFlags.HasFlag(AttachedHandFlags.Right))
            {
                _rightHandHaptics.TriggerHaptics(timeFromNow, duration, frequency, amplitude);
            }
        }

    }
}
