@echo off
glslangValidator test.frag.glsl -V -o test.frag.spv
glslangValidator test.vert.glsl -V -o test.vert.spv
glslangValidator tonemap.comp.glsl -V -o tonemap.comp.spv
glslangValidator shadowmap.vert.glsl -V -o shadowmap.vert.spv
glslangValidator shadowmap.frag.glsl -V -o shadowmap.frag.spv
move *.spv ../../EEData/Shaders
pause