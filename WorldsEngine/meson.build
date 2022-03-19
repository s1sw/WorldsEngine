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
subdir('Navigation')
subdir('PathTracer')
subdir('Physics')
subdir('Render')
subdir('Scripting')
subdir('Serialization')
subdir('Util')
subdir('VR')

cc = meson.get_compiler('cpp')

extern_dir = meson.source_root() + '/External'

lib_dir = extern_dir + '/Lib/' + host_machine.system() + '64dbg'

if get_option('buildtype').startswith('release')
  lib_dir = extern_dir + '/Lib/' + host_machine.system() + '64rel'
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
#vk_dep = dependency('vulkan')
embree_dep = dependency('embree', required: false)

if not embree_dep.found()
  embree_dep = declare_dependency(dependencies: cc.find_library('embree3', dirs: extern_dir + '/Lib/'))
endif

assimp_dep = dependency('assimp', required: false)

system_lib_dir = host_machine.system() + '64dbg'

if get_option('buildtype').startswith('release')
  system_lib_dir = host_machine.system() + '64rel'
endif

if not assimp_dep.found()
  cpp_comp = meson.get_compiler('cpp')
  assimp_dir = extern_dir + '/Assimp/' + system_lib_dir
  if get_option('buildtype').startswith('release')
    assimp_dir = extern_dir + '/Assimp/' + system_lib_dir
  endif
  assimp_inc_dir =  '../External/Assimp/include'

  assimp_lib = cpp_comp.find_library('assimp', dirs: assimp_dir)
  assimp_dep = declare_dependency(dependencies: assimp_lib, include_directories: include_directories(assimp_inc_dir))
endif


dl_dep = cc.find_library('dl', required : false)
openvr_dep = cc.find_library('openvr_api', dirs: lib_dir)
discord_csdk_dep = cc.find_library('discord_game_sdk', dirs: extern_dir + '/Lib')

platform_deps = []
platform_incdirs = []

if host_machine.system() == 'windows'
  platform_deps += cc.find_library('winmm')
  platform_deps += cc.find_library('ws2_32')
  platform_deps += cc.find_library('dwmapi')
  platform_deps += cc.find_library('gdiplus')
  platform_incdirs += '../External/WindowsOnlyInclude/'

  # FMOD
  if build_machine.system() == 'windows'
    platform_incdirs += include_directories('C:\\Program Files (x86)\\FMOD SoundSystem\\FMOD Studio API Windows\\api\\studio\\inc')
    platform_incdirs += include_directories('C:\\Program Files (x86)\\FMOD SoundSystem\\FMOD Studio API Windows\\api\\core\\inc')

    fmod_lib_dir = 'C:\\Program Files (x86)\\FMOD SoundSystem\\FMOD Studio API Windows\\api\\core\\lib\\x64'
    fmod_studio_lib_dir = 'C:\\Program Files (x86)\\FMOD SoundSystem\\FMOD Studio API Windows\\api\\studio\\lib\\x64'
  else
    platform_incdirs += get_option('fmod_api_path') + '/api/studio/inc'
    platform_incdirs += get_option('fmod_api_path') + '/api/core/inc'

    fmod_lib_dir = get_option('fmod_api_path') + '/api/core/lib/x64'
    fmod_studio_lib_dir = get_option('fmod_api_path') + '/api/studio/lib/x64'
  endif

  platform_deps += cc.find_library('fmod_vc', dirs: fmod_lib_dir)
  platform_deps += cc.find_library('fmodstudio_vc', dirs: fmod_studio_lib_dir)
endif

if host_machine.system() == 'linux'
  platform_deps += cc.find_library('tbb', required: false)
  platform_deps += cc.find_library('xcb')
  platform_deps += cc.find_library('xcb-ewmh')
  platform_deps += cc.find_library('xcb-image')

  # FMOD
  platform_incdirs += get_option('fmod_api_path') + '/api/studio/inc'
  platform_incdirs += get_option('fmod_api_path') + '/api/core/inc'

  fmod_lib_dir = get_option('fmod_api_path') + '/api/core/lib/x86_64'
  fmod_studio_lib_dir = get_option('fmod_api_path') + '/api/studio/lib/x86_64'

  platform_deps += cc.find_library('fmod', dirs: fmod_lib_dir)
  platform_deps += cc.find_library('fmodstudio', dirs: fmod_studio_lib_dir)
endif

steam_audio_dep = cc.find_library('phonon', dirs: extern_dir + '/Lib')

freetype_dep = dependency('freetype', required: false)

worlds_cpp_args = []
if get_option('enable_path_tracer')
  platform_deps += embree_dep
else
  platform_deps += steam_audio_dep
  worlds_cpp_args += '-DENABLE_STEAM_AUDIO'
endif

if get_option('enable_nv_aftermath')
  nv_aftermath_dep = cc.find_library('GFSDK_Aftermath_Lib.x64', dirs: extern_dir + '/Lib')
  platform_deps += nv_aftermath_dep
  worlds_cpp_args += '-DENABLE_NV_AFTERMATH'
endif

if not freetype_dep.found()
  freetype_dir = extern_dir + '/Lib/' + system_lib_dir

  freetype_lib = cc.find_library('freetype', dirs: freetype_dir)
  freetype_dep = declare_dependency(dependencies: freetype_lib)
endif

worlds = static_library('worlds', worlds_sources + ['tracy/TracyClient.cpp'],
  include_directories: [extern_include_dir, physx_include_dir, tracy_inc] + platform_incdirs,
  dependencies: [
    assimp_dep, physfs_dep, sdl2_dep, physx_deps,
    dl_dep, openvr_dep, wmdl_dep, wtex_dep, discord_csdk_dep,
    discord_sdk_dep,
    slib_dep, platform_deps, #steam_audio_dep,
    recast_dep, detour_dep,
    freetype_dep, embree_dep
    ],
  link_args: ['-rdynamic'],
  cpp_args: worlds_cpp_args
)

worlds_dep = declare_dependency(include_directories: worlds_inc, link_with: worlds)
