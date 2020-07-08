@echo off
glslangValidator test.frag.glsl -V -o test.frag.spv
glslangValidator test.vert.glsl -V -o test.vert.spv
glslangValidator sdf.comp.glsl -V -o sdf.comp.spv
glslangValidator tonemap.comp.glsl -V -o tonemap.comp.spv
glslangValidator shadowmap.vert.glsl -V -o shadowmap.vert.spv
glslangValidator shadowmap.frag.glsl -V -o shadowmap.frag.spv
glslangValidator voxelobj.vert.glsl -V -o voxelobj.vert.spv
glslangValidator voxelobj_shadow.vert.glsl -V -o voxelobj_shadow.vert.spv
glslangValidator voxelobj.frag.glsl -V -o voxelobj.frag.spv
pause