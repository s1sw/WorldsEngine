![logo](EngineData/UI/Editor/Images/logo_no_background.png)
# WorldsEngine

[![forthebadge](https://forthebadge.com/images/badges/powered-by-black-magic.svg)](https://forthebadge.com)

A C++ and Vulkan game engine that's been my hobby project for the past couple years. Don't count on building anything off of it (or getting it to build for that matter)!
It's MIT licensed, so if you find something that is in fact useful feel free to take it as long as you include the notice :)

## Building Instructions (Windows)

These may or may not work, best of luck!

1. Install Meson, Python, Ninja, VS2022 and [the FMOD engine](https://www.fmod.com/download#fmodstudio).
2. Run the `generate_meson_build.py` script under the WorldsEngine folder.
3. Open a VS x64 native tools command prompt, go to the root of the repository and run `meson setup build_ninja`. This is quite slow, you can speed up the SDL2 part significantly by turning off real-time protection in Windows Security
4. `cd` into the directory and run `meson configure -Dcpp_std=c++latest -Dsdl2:default_library=static -Db_vscrt=mtd -Dphysfs:default_library=static`.
5. Run `ninja` to compile.
6. Copy `freetype.dll`, `GPUUtilities.dll`, `openvr_api.dll`, `phonon.dll`, `phonon_fmod.dll`, `TrueAudioNext.dll` from `External/Bin` to the `LonelyGalaxy` folder.
7. Copy `assimp-vc142-mt.dll` and `zlib1.dll` from `External/Assimp/windows64rel` to the `LonelyGalaxy` folder.
8. Download the [.NET 6.0 SDK binaries](https://dotnet.microsoft.com/en-us/download/dotnet/6.0) and copy everything in `dotnet-sdk-6.0.400-win-x64/shader/Microsoft.NETCore.App/6.0.8` to a new folder called `NetAssemblies` in the `LonelyGalaxy` folder.
9. cd to the `WorldsEngineManaged` directory and run `dotnet build`.
10. cd to `EngineSrcData/Shaders` and run `.\BuildTools\ShaderBuilder.exe`
11. Symlink or copy the `EngineData` folder into the `LonelyGalaxy` folder.
12. Run `.\lonelygalaxy.exe --novr --editor` to launch the editor.

Yes this sucks. I'm sorry.
