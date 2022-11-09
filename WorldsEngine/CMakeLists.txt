cmake_minimum_required(VERSION 3.15)

include(FetchContent)
option(WORLDS_USE_ASSIMP "Build with Assimp to import models." ON)
option(WORLDS_USE_OPENXR "Build with OpenXR for VR support." ON)
option(WORLDS_BUILD_EDITOR "Build the editor." ON)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL
    GIT_TAG 53dea9830964eee8b5c2a7ee0a65d6e268dc78a1 # 2.0.22
)

FetchContent_Declare(
    PHYSFS
    GIT_REPOSITORY https://github.com/icculus/physfs
    GIT_TAG 0d4e9aac4575744ddaae56b146f1be19f064f0e5
)

FetchContent_Declare(
    TRACY
    GIT_REPOSITORY https://github.com/wolfpld/tracy
    GIT_TAG a8511d357650282c6915f7aa9775e9c4945e42b5
)

FetchContent_Declare(
    SLIB
    GIT_REPOSITORY https://github.com/someonesomewheredev/slib
    GIT_TAG 5045e863fd29fd0877d7d73cc008b826ae58f982
)

FetchContent_Declare(
    RECASTDETOUR
    GIT_REPOSITORY https://github.com/recastnavigation/recastnavigation
    GIT_TAG c187b7e88bfc922bca2175e3751881bdf378ff0d
)

FetchContent_Declare(
    ASSIMP
    GIT_REPOSITORY https://github.com/assimp/assimp
    GIT_TAG c8dafe0d2887242285c0080c6cbbea8c1f1c8094
)

FetchContent_Declare(
    ENKITS
    GIT_REPOSITORY https://github.com/dougbinks/enkiTS
    GIT_TAG fd5a2603a0adf1884eee40dd081e30733a3b46dd 
)

set(RECASTNAVIGATION_DEMO off CACHE BOOL "" FORCE)
# recasts' tests don't even build on C++20
set(RECASTNAVIGATION_TESTS off CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_TESTS off CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS off CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT off CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL off CACHE BOOL "" FORCE)

set(ENKITS_BUILD_C_INTERFACE off CACHE BOOL "" FORCE)
set(ENKITS_BUILD_EXAMPLES off CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2 PHYSFS TRACY SLIB RECASTDETOUR ENKITS)

if(WORLDS_USE_ASSIMP)
    FetchContent_MakeAvailable(ASSIMP)
endif()

project(EngineLibrary)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

include("Audio/CMakeLists.txt")
include("ComponentMeta/CMakeLists.txt")
include("Core/CMakeLists.txt")
include("ImGui/CMakeLists.txt")
include("Input/CMakeLists.txt")
include("IO/CMakeLists.txt")
include("Libs/CMakeLists.txt")
include("Navigation/CMakeLists.txt")
include("Physics/CMakeLists.txt")
include("Render/CMakeLists.txt")
include("Scripting/CMakeLists.txt")
include("Serialization/CMakeLists.txt")
include("Util/CMakeLists.txt")
include("VR/CMakeLists.txt")

if(WORLDS_BUILD_EDITOR)
    include("AssetCompilation/CMakeLists.txt")
    include("Editor/CMakeLists.txt")
endif()

set(
    wsrc 
    ${worlds_audio_src} ${worlds_componentmeta_src} ${worlds_core_src} 
    ${worlds_imgui_src} ${worlds_input_src} ${worlds_io_src} ${worlds_libs_src}
    ${worlds_navigation_src} ${worlds_physics_src} ${worlds_render_src}
    ${worlds_scripting_src} ${worlds_serialization_src} ${worlds_util_src}
    ${worlds_vr_src}
)

if(WORLDS_BUILD_EDITOR)
    list(APPEND wsrc ${worlds_editor_src} ${worlds_assetcompilation_src})
endif()

add_library(${PROJECT_NAME} STATIC ${wsrc})
target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_STEAM_AUDIO)

if(WORLDS_USE_ASSIMP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_ASSIMP)
    target_link_libraries(${PROJECT_NAME} assimp::assimp)
endif()

if(WORLDS_USE_OPENXR)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_OPENXR)
endif()

if(WORLDS_BUILD_EDITOR)
    target_compile_definitions(${PROJECT_NAME} PUBLIC BUILD_EDITOR)
endif()

if(WIN32)
    set(LIB_PATH ../External/Lib/windows64$<IF:$<CONFIG:Debug>,dbg,rel>)
    target_link_libraries(${PROJECT_NAME} dbghelp gdiplus dwmapi "${LIB_PATH}/freetype")
    target_link_libraries(${PROJECT_NAME}
        "${LIB_PATH}/PhysX_64"
        "${LIB_PATH}/PhysXCommon_64"
        "${LIB_PATH}/PhysXCooking_64"
        "${LIB_PATH}/PhysXExtensions_static_64"
        "${LIB_PATH}/PhysXFoundation_64"
        "${LIB_PATH}/PhysXTask_static_64"
        "${LIB_PATH}/SceneQuery_static_64"
    )
    target_link_libraries(${PROJECT_NAME} "${FMOD_DIR}/core/lib/x64/fmod_vc.lib" "${FMOD_DIR}/studio/lib/x64/fmodstudio_vc.lib")
    target_link_libraries(${PROJECT_NAME} "../External/Lib/phonon")

    if(WORLDS_USE_OPENXR)
        target_link_libraries(${PROJECT_NAME} "${LIB_PATH}/openxr_loader")
    endif()
endif()

find_package(Vulkan REQUIRED)

target_link_libraries(${PROJECT_NAME} R2 SDL2::SDL2 physfs TracyClient slib Recast Detour enkiTS)
target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PUBLIC . ../External/Include ../External/Include/physx ../TextureFormat ../ModelFormat)
target_include_directories(${PROJECT_NAME} PUBLIC "${FMOD_DIR}/core/inc" "${FMOD_DIR}/studio/inc")
