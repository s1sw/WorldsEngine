namespace WorldsEngine
{
    public interface ISystem
    {
        void OnSceneStart() {}
        void OnUpdate(float deltaTime) {}
        void OnSimulate(float deltaTime) {}
    }
}
