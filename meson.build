project('worldseng', 'cpp', 'c', default_options: [
  'c_std=c11',
  'cpp_std=c++2a',
  'win_subsystem=windows'
])

if get_option('buildtype').startswith('debug')
  add_global_arguments('-D_DEBUG=1', language: 'cpp')
  add_global_arguments('-DWREN_DEBUG_TRACE_GC=1', language: ['cpp','c'])
endif

if get_option('buildtype').startswith('release')
  add_global_arguments('-DNDEBUG=1', language: 'cpp')
endif

if get_option('enable_discord_rpc')
  add_global_arguments('-DDISCORD_RPC=1', language: 'cpp')
endif

if host_machine.system() == 'windows'
  add_global_arguments('-DNOMINMAX', language: 'cpp')
endif

add_global_arguments('-DPX_PHYSX_STATIC_LIB', language: 'cpp')
add_global_arguments('-DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1', language: 'cpp')
add_global_arguments('-DVK_NO_PROTOTYPES', language: 'cpp')

cpp_compiler = meson.get_compiler('cpp')

if cpp_compiler.get_argument_syntax() == 'gcc'
  add_global_arguments('-ffast-math', language: 'cpp')
  add_global_arguments('-fno-rtti', language: 'cpp')
  add_global_arguments('-Wno-volatile', language: 'cpp') # remove if we ever move off of glm (unlikely)
  add_global_arguments('-g', language: 'cpp') # generate debug symbols even in release
elif cpp_compiler.get_argument_syntax() == 'msvc'
  add_global_arguments('/fp:fast', language: 'cpp')
  add_global_arguments('/arch:AVX', language: 'cpp')
endif

extern_include_dir = include_directories('External/Include')
# THIS IS SO INCREDIBLY STUPID
# WHY MUST YOU BE THIS WAY PHYSX
physx_include_dir = include_directories('External/Include/physx')
sdl2_dep = dependency('sdl2', fallback: ['sdl2', 'sdl2_dep'])

subdir('ModelFormat')
subdir('Tools')
subdir('DiscordSDK')
subdir('WorldsEngine')
subdir('LonelyGalaxy')
