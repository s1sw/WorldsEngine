using WorldsEngine;

namespace Game
{
    internal class DebugGlobals
    {
        public static bool AIIgnorePlayer = false;
        public static bool PlayerInvincible = false;

        [ConsoleCommand("game_toggleAiIgnore", "Toggles AI ignoring the player.")]
        public static void ToggleAIIgnore(string args)
        {
            AIIgnorePlayer = !AIIgnorePlayer;
        }

        [ConsoleCommand("god")]
        public static void TogglePlayerInvincibility(string args)
        {
            PlayerInvincible = !PlayerInvincible;
            if (PlayerInvincible)
                Log.Msg("player is now invincible");
            else
                Log.Msg("player is no longer invincible");
        }
    }
}
