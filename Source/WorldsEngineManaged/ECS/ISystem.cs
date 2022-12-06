namespace WorldsEngine
{
    public interface ISystem
    {
        /// <summary>
        /// Called when the scene is loaded or play is pressed.
        /// </summary>
        void OnSceneStart() {}

        /// <summary>
        /// Called every frame. Use this for user-facing things like UI and reading input.
        /// </summary>
        void OnUpdate() {}

        /// <summary>
        /// Called every simulation tick. Use this for things like movement, enemy AI, etc.
        /// </summary>
        void OnSimulate() {}
    }
}
