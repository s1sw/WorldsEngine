![logo](EngineData/UI/Editor/Images/logo_no_background.png)
# WorldsEngine

[![forthebadge](https://forthebadge.com/images/badges/powered-by-black-magic.svg)](https://forthebadge.com)

A C++ and Vulkan game engine that's been my hobby project for the past couple years. Don't count on building anything off of it (or getting it to build for that matter)!
It's MIT licensed, so if you find something that is in fact useful feel free to take it as long as you include the notice :)

## Building Instructions (Windows)

1. Install Cmake, Ninja, VS2022 and [the FMOD engine](https://www.fmod.com/download#fmodstudio).
2. Create a new directory, cd into it and run `cmake .. -G Ninja`
3. Run `ninja` to compile. If you get an error saying "A required privilege is not held by the client" enable Developer mode in Windows settings (it's required to make symlinks).
4. cd to the `WorldsEngineManaged` directory and run `dotnet build`.
5. cd to `EngineSrcData/Shaders` and run `.\BuildTools\ShaderBuilder.exe`
6. Run `.\lonelygalaxy.exe --novr --editor` to launch the editor.
