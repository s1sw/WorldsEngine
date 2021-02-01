wmdl_convert_sources = [
  'convert.cpp'
]

assimp_dep = dependency('assimp', required: false)

if assimp_dep.found()
	wmdl_convert = executable('wmdl_convert', sources: wmdl_convert_sources, dependencies: [assimp_dep, wmdl_dep])
endif
