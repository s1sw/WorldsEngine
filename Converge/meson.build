sources = [
    'Events.cpp',
    'ConvergeEventHandler.cpp',
    'LocospherePlayerSystem.cpp',
    'Main.cpp',
    'resource.h',
    'ConvergeEventHandler.hpp',
    'LocospherePlayerSystem.hpp',
    'PidController.hpp',
    'PhysHandSystem.hpp',
    'PhysHandSystem.cpp',
    'MathsUtil.hpp',
    'MathsUtil.cpp',
    'DebugArrow.cpp',
    'DebugArrow.hpp',
    'ObjectParentSystem.cpp',
    'ObjectParentSystem.hpp',
    'Client.hpp',
    'Client.cpp',
    'Server.hpp',
    'Server.cpp',
    'Network.hpp',
    'NetBase.cpp',
    'NetMessage.hpp',
    'NetMessage.cpp'
]

crash_handler_deps = []

if host_machine.system() == 'windows'
  cpp_compiler = meson.get_compiler('cpp')
  crash_handler_deps += cpp_compiler.find_library('dbghelp')
endif

executable('converge', sources, 
  include_directories: [physx_include_dir, extern_include_dir], 
  dependencies: [worlds_dep, discord_sdk_dep, wren_dep, vk_dep, enet_dep, sdl2_dep, physfs_dep, crash_handler_deps], win_subsystem: 'windows')

