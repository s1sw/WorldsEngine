using WorldsEngine;

namespace Game
{
    internal class DebugGlobals
    {
        public static bool AIIgnorePlayer = false;

        [ConsoleCommand("game_toggleAiIgnore", "Toggles AI ignoring the player.")]
        public static void RegisterCommands(string args)
        {
            AIIgnorePlayer = !AIIgnorePlayer;
        }
    }
}
