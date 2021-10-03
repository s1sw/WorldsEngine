using WorldsEngine;

namespace Game
{
    [Component]
    class FlickeringLight : IThinkingComponent, IStartListener
    {
        // Flicker strings go A-Z, where A is minimum brightness and Z is 2x.
        // This string matches the "Fluorescent Flicker" pattern from Quake.
        public string FlickerString = "mmamammmmammamamaaamammma";
        public float FlickerSpeed = 1.0f;
        public int Offset = 0;

        private float _initialIntensity = 0.0f;

        public void Start(Entity e)
        {
            var worldLight = Registry.GetComponent<WorldLight>(e);
            _initialIntensity = worldLight.Intensity;
        }

        public void Think(Entity e)
        {
            int index = (int)((Time.CurrentTime * 10 * FlickerSpeed + Offset) % FlickerString.Length);

            int flickerVal = FlickerString[index] - 'a';

            float intensity = flickerVal / 13.0f;

            var worldLight = Registry.GetComponent<WorldLight>(e);
            worldLight.Intensity = _initialIntensity * intensity;
        }
    }
}
