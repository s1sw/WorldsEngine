@echo off
glslangValidator -g test.frag.glsl -V -o test.frag.spv
glslangValidator -g test.vert.glsl -V -o test.vert.spv
glslangValidator -g tonemap.comp.glsl -V -o tonemap.comp.spv
glslangValidator -g shadowmap.vert.glsl -V -o shadowmap.vert.spv
glslangValidator -g shadowmap.frag.glsl -V -o shadowmap.frag.spv
glslangValidator -g wire_obj.vert.glsl -V -o wire_obj.vert.spv
glslangValidator -g wire_obj.frag.glsl -V -o wire_obj.frag.spv
move *.spv ../../EEData/Shaders
pause