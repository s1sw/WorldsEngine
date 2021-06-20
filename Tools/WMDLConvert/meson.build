wmdl_convert_sources = [
  'convert.cpp',
  'mikktspace.c',
  'weldmesh.cpp'
]

assimp_dep = dependency('assimp', required: false)

if not assimp_dep.found()
  cpp_comp = meson.get_compiler('cpp')
    assimp_dir = meson.project_source_root() + '/External/Assimp/' + host_machine.system() + '64dbg'
    if get_option('buildtype').startswith('release')
        assimp_dir = meson.project_source_root() + '/External/Assimp/' + host_machine.system() + '64rel'
    endif
    assimp_inc_dir = '../../External/Assimp/include'

    assimp_lib = cpp_comp.find_library('assimp', dirs: assimp_dir)
    assimp_dep = declare_dependency(dependencies: assimp_lib, include_directories: include_directories(assimp_inc_dir))
endif

if assimp_dep.found()
  wmdl_convert = executable('wmdl_convert',
    sources: wmdl_convert_sources,
    dependencies: [assimp_dep, wmdl_dep],
    include_directories: extern_include_dir)
endif
