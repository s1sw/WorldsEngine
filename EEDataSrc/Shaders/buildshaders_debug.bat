@echo off
glslangValidator -g test.frag.glsl -V -o test.frag.spv
glslangValidator -g test.vert.glsl -V -o test.vert.spv
glslangValidator -g sdf.comp.glsl -V -o sdf.comp.spv
glslangValidator -g tonemap.comp.glsl -V -o tonemap.comp.spv
glslangValidator -g shadowmap.vert.glsl -V -o shadowmap.vert.spv
glslangValidator -g shadowmap.frag.glsl -V -o shadowmap.frag.spv
glslangValidator -g voxelobj.vert.glsl -V -o voxelobj.vert.spv
glslangValidator -g voxelobj.frag.glsl -V -o voxelobj.frag.spv
pause