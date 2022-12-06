using WorldsEngine;
using WorldsEngine.Input;

namespace {{PROJECT_NAME}};

class ExampleSystem : ISystem
{
    public void OnSceneStart()
    {
        Log.Msg("The scene has started!");
    }

    public void OnUpdate()
    {
        if (Keyboard.KeyPressed(KeyCode.W))
        {
            Log.Msg("W pressed!");
        }
    }
}