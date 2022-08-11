cmake_minimum_required(VERSION 3.15)

project(EngineLoader)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
file(GLOB lsrc ./**.cpp)

add_executable(EngineLoader ${lsrc})
target_link_libraries(EngineLoader PUBLIC EngineLibrary)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:EngineLoader>
)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:physfs> $<TARGET_FILE_DIR:EngineLoader>
)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Recast> $<TARGET_FILE_DIR:EngineLoader>
)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Detour> $<TARGET_FILE_DIR:EngineLoader>
)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:TracyClient> $<TARGET_FILE_DIR:EngineLoader>
)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:assimp::assimp> $<TARGET_FILE_DIR:EngineLoader>
)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${PROJECT_SOURCE_DIR}/../External/bin/openvr_api.dll" $<TARGET_FILE_DIR:EngineLoader>
)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${PROJECT_SOURCE_DIR}/../External/bin/$<IF:$<CONFIG:Debug>,win64dbg,win64rel>/freetype.dll" $<TARGET_FILE_DIR:EngineLoader>
)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${FMOD_DIR}/core/lib/x64/fmod.dll" $<TARGET_FILE_DIR:EngineLoader>
)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${FMOD_DIR}/studio/lib/x64/fmodstudio.dll" $<TARGET_FILE_DIR:EngineLoader>
)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${FMOD_DIR}/studio/lib/x64/fmodstudio.dll" $<TARGET_FILE_DIR:EngineLoader>
)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    "${PROJECT_SOURCE_DIR}/../EngineData" $<TARGET_FILE_DIR:EngineLoader>/EngineData
)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    "${PROJECT_SOURCE_DIR}/../NetAssemblies" $<TARGET_FILE_DIR:EngineLoader>/NetAssemblies
)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${PROJECT_SOURCE_DIR}/imgui.ini" $<TARGET_FILE_DIR:EngineLoader>/imgui.ini
)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${PROJECT_SOURCE_DIR}/imgui_editor.ini" $<TARGET_FILE_DIR:EngineLoader>/imgui_editor.ini
)

add_custom_command (TARGET EngineLoader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${PROJECT_SOURCE_DIR}/StartEditor.bat" $<TARGET_FILE_DIR:EngineLoader>/StartEditor.bat
)