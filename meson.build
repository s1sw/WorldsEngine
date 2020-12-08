project('worldseng', 'cpp', 'c', default_options: [
  'cpp_std=c++2a', 
  'c_std=c11',
  'backend_startup_project=Converge'
])

extern_include_dir = include_directories('External/Include')
# THIS IS SO INCREDIBLY STUPID
# WHY MUST YOU BE THIS WAY PHYSX
physx_include_dir = include_directories('External/Include/physx') 
sdl2_dep = dependency('sdl2')

if get_option('buildtype').startswith('debug')
  add_global_arguments('-D_DEBUG=1', language: 'cpp')
endif

if get_option('buildtype').startswith('release')
  add_global_arguments('-DNDEBUG=1', language: 'cpp')
endif

if get_option('enable_discord_rpc')
  add_global_arguments('-DDISCORD_RPC=1', language: 'cpp')
endif

add_global_arguments('-ffast-math', language: 'cpp')
add_global_arguments('-g', language: 'cpp') # generate debug symbols even in release

subdir('ModelFormat')
subdir('Tools')
subdir('DiscordSDK')
subdir('ExperimentalEngine')
subdir('Converge')
