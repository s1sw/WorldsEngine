sources = [
    'EventHandler.cpp',
    'Main.cpp',
    'MathsUtil.cpp',
    'DebugArrow.cpp',
    'ObjectParentSystem.cpp',

    'DebugArrow.hpp',
    'EventHandler.hpp',
    'MathsUtil.hpp',
    'ObjectParentSystem.hpp',
]

crash_handler_deps = []
lg_link_args = []

if host_machine.system() == 'windows'
  cpp_compiler = meson.get_compiler('cpp')
  crash_handler_deps += cpp_compiler.find_library('dbghelp')
  sources += import('windows').compile_resources('LonelyGalaxy.rc', depend_files: 'Manifest.xml')
else
  lg_link_args += '-rdynamic'
endif

executable('lonelygalaxy', sources,
  include_directories: [physx_include_dir, extern_include_dir, platform_incdirs],
  dependencies: [slib_dep, worlds_dep, discord_sdk_dep, sdl2_dep, physfs_dep, crash_handler_deps, freetype_dep], win_subsystem: 'windows', link_args: lg_link_args)

