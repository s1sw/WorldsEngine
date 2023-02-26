## This project has now moved to [GitLab](https://gitlab.com/someonesomewhere167/worlds-engine)

![logo](EngineData/UI/Editor/Images/logo_no_background.png)
# Worlds Engine

![Editor screenshot showing a quadcopter firing a selected laser object](Docs/Screenshots/EditorTH.png)
![Editor screenshot showing gun selected in the editor from multiple different angles](Docs/Screenshots/EditorPR.png)

A C++ and Vulkan game engine that's been my hobby project for the past couple years after originally starting as a
Minecraft clone.

Currently only Windows is supported as a platform. I hope to support Linux in the future however, pending:
- fixes to compile on GCC/Clang
- compiling PhysX 5 on Linux
- sorting out OS abstractions (mainly open file dialogs)

## Features

- C# hotloading (even during gameplay)
- Tile-based forward+ Vulkan renderer based on a custom abstraction layer
- Full editor workflow with the ability to edit and inspect the game in realtime
- Asset pipeline compiling textures+models to a suitable runtime format
- PhysX integration for physics, FMOD and Steam Audio integration for audio and Recast/Detour integration for navigation

## Building Instructions (Windows)

1. Install Cmake, Ninja, VS2022 and [the FMOD engine](https://www.fmod.com/download#fmodstudio).
2. Create a new directory, cd into it and run `cmake .. -G Ninja`
3. Run `ninja` to compile. If you get an error saying "A required privilege is not held by the client" enable Developer mode in Windows settings (it's required to make symlinks).
4. cd to the `WorldsEngineManaged` directory and run `dotnet build`.
5. cd to `EngineSrcData/Shaders` and run `.\BuildTools\ShaderBuilder.exe`
6. In the build directory, run `.\BuildOutput\StartEditor.bat` to launch the editor.
