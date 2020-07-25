@echo off
cd /D "%~dp0"
glslangValidator test.frag.glsl -V -o test.frag.spv
glslangValidator test.vert.glsl -V -o test.vert.spv
glslangValidator tonemap.comp.glsl -V -o tonemap.comp.spv
glslangValidator shadowmap.vert.glsl -V -o shadowmap.vert.spv
glslangValidator shadowmap.frag.glsl -V -o shadowmap.frag.spv
glslangValidator wire_obj.vert.glsl -V -o wire_obj.vert.spv
glslangValidator wire_obj.frag.glsl -V -o wire_obj.frag.spv
move *.spv ../../EEData/Shaders