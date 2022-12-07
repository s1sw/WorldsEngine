![logo](EngineData/UI/Editor/Images/logo_no_background.png)
# WorldsEngine

![Editor screenshot](Docs/Screenshots/EditorTH.png)

A C++ and Vulkan game engine that's been my hobby project for the past couple years. Don't count on building anything off of it (or getting it to build for that matter)!
It's MIT licensed, so if you find something that is in fact useful feel free to take it as long as you include the notice :)

## Features

- C# hotloading (even during gameplay)
- Forward+ Vulkan renderer
- PhysX integration for physics, FMOD and Steam Audio integration for audio and Recast/Detour integration for AI

## Building Instructions (Windows)

1. Install Cmake, Ninja, VS2022 and [the FMOD engine](https://www.fmod.com/download#fmodstudio).
2. Create a new directory, cd into it and run `cmake .. -G Ninja`
3. Run `ninja` to compile. If you get an error saying "A required privilege is not held by the client" enable Developer mode in Windows settings (it's required to make symlinks).
4. cd to the `Source/WorldsEngineManaged` directory and run `dotnet build`.
5. cd to `EngineSrcData/Shaders` and run `.\BuildTools\ShaderBuilder.exe`
6. Run `.\BuildOutput\WorldsEngine.exe --novr --editor` to launch the editor.
