worlds_sources = []

subdir('AssetCompilation')
subdir('Audio')
subdir('ComponentMeta')
subdir('Core')
subdir('Editor')
subdir('ImGui')
subdir('Input')
subdir('IO')
subdir('Libs')
subdir('Physics')
subdir('Render')
subdir('Scripting')
subdir('Serialization')
subdir('Util')
subdir('VR')

cc = meson.get_compiler('cpp')

lib_dir = meson.project_source_root() + '/External/Lib/' + host_machine.system() + '64dbg'

if get_option('buildtype').startswith('release')
  lib_dir = meson.project_source_root() + '/External/Lib/' + host_machine.system() + '64rel'
endif

physx_deps = [
  cc.find_library('PhysXCommon_static_64', dirs: lib_dir),
  cc.find_library('PhysXCooking_static_64', dirs: lib_dir),
  cc.find_library('PhysXExtensions_static_64', dirs: lib_dir),
  cc.find_library('PhysXFoundation_static_64', dirs: lib_dir),
  cc.find_library('PhysXPvdSDK_static_64', dirs: lib_dir),
  cc.find_library('PhysX_static_64', dirs: lib_dir),
  cc.find_library('PhysXVehicle_static_64', dirs: lib_dir)
]

worlds_inc = include_directories('.')
tracy_inc = include_directories('./tracy/')
physfs_dep = dependency('physfs', fallback: ['physfs', 'physfs_dep'])
slib_dep = dependency('slib', fallback: ['slib', 'slib_dep'])
vk_dep = dependency('vulkan')
tbb_dep = cc.find_library('tbb', required: false)

enet_subproj = subproject('enet')

enetlib = enet_subproj.get_variable('enetlib')
enet_inc = enet_subproj.get_variable('incdir')

enet_dep = declare_dependency(link_with: enetlib, include_directories: enet_inc)

dl_dep = cc.find_library('dl', required : false)
openvr_dep = cc.find_library('openvr_api', dirs: lib_dir)
discord_csdk_dep = cc.find_library('discord_game_sdk', dirs: meson.project_source_root() + '/External/Lib')

wren_dep = dependency('wren', fallback: ['wren', 'wren_dep'])

platform_deps = []

if host_machine.system() == 'windows'
  platform_deps += cc.find_library('winmm')
  platform_deps += cc.find_library('ws2_32')
endif

steam_audio_dep = cc.find_library('phonon', dirs: meson.project_source_root() + '/External/Lib')

worlds = static_library('worlds', worlds_sources,
  include_directories: [extern_include_dir, physx_include_dir, tracy_inc],
  dependencies: [
    physfs_dep, sdl2_dep, physx_deps,
    dl_dep, openvr_dep, wmdl_dep, discord_csdk_dep,
    discord_sdk_dep, tbb_dep, wren_dep, enet_dep,
    slib_dep, platform_deps, steam_audio_dep
  ]
)
worlds_dep = declare_dependency(include_directories: worlds_inc, link_with: worlds)
