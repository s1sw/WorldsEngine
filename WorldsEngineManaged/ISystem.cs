namespace WorldsEngine
{
    public interface ISystem
    {
        void OnSceneStart() {}
        void OnUpdate() {}
        void OnSimulate() {}
    }
}
