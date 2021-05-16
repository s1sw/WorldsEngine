sources = [
    'Client.cpp',
    'CustomComponentMeta.cpp',
    'Events.cpp',
    'EventHandler.cpp',
    'GameClient.cpp',
    'GameServer.cpp',
    'LocospherePlayerSystem.cpp',
    'Main.cpp',
    'MultiplayerManager.cpp',
    'MathsUtil.cpp',
    'DebugArrow.cpp',
    'ObjectParentSystem.cpp',
    'PhysHandSystem.cpp',
    'PlayerGrabManager.cpp',
    'Server.cpp',
    'NetBase.cpp',
    'NetMessage.cpp',

    'Client.hpp',
    'DebugArrow.hpp',
    'ContactDamageDealer.hpp',
    'EventHandler.hpp',
    'GameClient.hpp',
    'GameServer.hpp',
    'GripPoint.hpp',
    'LocospherePlayerSystem.hpp',
    'MathsUtil.hpp',
    'MultiplayerManager.hpp',
    'NetMessage.hpp',
    'Network.hpp',
    'ObjectParentSystem.hpp',
    'PhysHandSystem.hpp',
    'PlayerGrabManager.hpp',
    'PidController.hpp',
    'Server.hpp',
    'SyncedRB.hpp'
]

crash_handler_deps = []

if host_machine.system() == 'windows'
  cpp_compiler = meson.get_compiler('cpp')
  crash_handler_deps += cpp_compiler.find_library('dbghelp')
  sources += import('windows').compile_resources('LonelyGalaxy.rc')
endif

executable('lonelygalaxy', sources, 
  include_directories: [physx_include_dir, extern_include_dir], 
  dependencies: [worlds_dep, discord_sdk_dep, wren_dep, enet_dep, sdl2_dep, physfs_dep, crash_handler_deps], win_subsystem: 'windows')

