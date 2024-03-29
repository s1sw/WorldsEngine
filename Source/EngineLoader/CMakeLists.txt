cmake_minimum_required(VERSION 3.15)

project(EngineLoader)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
file(GLOB lsrc ./**.cpp)

add_executable(${PROJECT_NAME} WIN32 ${lsrc})
set_target_properties(${PROJECT_NAME}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/BuildOutput"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/BuildOutput"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/BuildOutput"
)

target_link_libraries(${PROJECT_NAME} PUBLIC EngineLibrary SDL2::SDL2main)

if (WIN32)
    target_sources(${PROJECT_NAME} PRIVATE WorldsEngine.rc)
endif ()

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:physfs> $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Recast> $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Detour> $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:TracyClient> $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

if (WORLDS_USE_ASSIMP)
    add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:assimp::assimp> $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif ()

set(BIN_DIR_U ${PROJECT_SOURCE_DIR}/../../External/bin/win64)
set(BIN_DIR ${BIN_DIR_U}/$<IF:$<CONFIG:Debug>,debug,release>)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${BIN_DIR_U}/openxr_loader.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${BIN_DIR_U}/phonon.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${BIN_DIR_U}/phonon_fmod.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${BIN_DIR}/freetype.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${BIN_DIR}/freetype.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${BIN_DIR}/PhysX_64.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${BIN_DIR}/PhysXCommon_64.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${BIN_DIR}/PhysXCooking_64.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${BIN_DIR}/PhysXFoundation_64.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${FMOD_DIR}/core/lib/x64/fmod.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${FMOD_DIR}/studio/lib/x64/fmodstudio.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${FMOD_DIR}/studio/lib/x64/fmodstudio.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    "${PROJECT_SOURCE_DIR}/../../EngineData" $<TARGET_FILE_DIR:${PROJECT_NAME}>/EngineData
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    "${PROJECT_SOURCE_DIR}/../../NetAssemblies" $<TARGET_FILE_DIR:${PROJECT_NAME}>/NetAssemblies
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${PROJECT_SOURCE_DIR}/imgui.ini" $<TARGET_FILE_DIR:${PROJECT_NAME}>/imgui.ini
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${PROJECT_SOURCE_DIR}/imgui_editor.ini" $<TARGET_FILE_DIR:${PROJECT_NAME}>/imgui_editor.ini
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${PROJECT_SOURCE_DIR}/StartEditor.bat" $<TARGET_FILE_DIR:${PROJECT_NAME}>/StartEditor.bat
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${PROJECT_SOURCE_DIR}/blender_glbexport.py" $<TARGET_FILE_DIR:${PROJECT_NAME}>/blender_glbexport.py
)

add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROJECT_SOURCE_DIR}/ProjectTemplate" $<TARGET_FILE_DIR:${PROJECT_NAME}>/ProjectTemplate
) 

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "WorldsEngine")
