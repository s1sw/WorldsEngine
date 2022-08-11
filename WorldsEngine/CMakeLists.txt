cmake_minimum_required(VERSION 3.15)

include(FetchContent)

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
    GIT_TAG d2e5ca7594878175e594a2e89574f7f2b81ec25a
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
set(RECASTNAVIGATION_DEMO off CACHE BOOL "" FORCE)
# recasts' tests don't even build on C++20
set(RECASTNAVIGATION_TESTS off CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_TESTS off CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS off CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2 PHYSFS TRACY SLIB RECASTDETOUR ASSIMP)

project(EngineLibrary)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
file(GLOB_RECURSE wsrc ./*.cpp ./*.cc ./*.c)

add_library(${PROJECT_NAME} STATIC ${wsrc})

if(WIN32)
    set(LIB_PATH ../External/Lib/windows64dbg)
    target_compile_definitions(${PROJECT_NAME} PUBLIC PX_PHYSX_STATIC_LIB)
    target_link_libraries(${PROJECT_NAME} dbghelp gdiplus dwmapi "${LIB_PATH}/openvr_api" "${LIB_PATH}/freetype")
    target_link_libraries(${PROJECT_NAME}
        "${LIB_PATH}/PhysXCommon_static_64"
        "${LIB_PATH}/PhysXCooking_static_64"
        "${LIB_PATH}/PhysXExtensions_static_64"
        "${LIB_PATH}/PhysXFoundation_static_64"
        "${LIB_PATH}/PhysXPvdSDK_static_64"
        "${LIB_PATH}/PhysX_static_64"
        "${LIB_PATH}/PhysXVehicle_static_64"
    )
endif()

target_link_libraries(${PROJECT_NAME} R2 SDL2::SDL2 physfs TracyClient slib Recast Detour assimp::assimp)
target_link_libraries(${PROJECT_NAME} "${FMOD_DIR}/core/lib/x64/fmod_vc.lib" "${FMOD_DIR}/studio/lib/x64/fmodstudio_vc.lib")
target_include_directories(${PROJECT_NAME} PUBLIC . ../External/Include ../TextureFormat ../ModelFormat)
target_include_directories(${PROJECT_NAME} PUBLIC "${FMOD_DIR}/core/inc" "${FMOD_DIR}/studio/inc")
