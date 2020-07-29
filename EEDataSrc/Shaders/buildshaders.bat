@echo off
cd /D "%~dp0"
glslangValidator standard.frag.glsl -V -o standard.frag.spv
glslangValidator standard.vert.glsl -V -o standard.vert.spv
glslangValidator tonemap.comp.glsl -V -o tonemap.comp.spv
glslangValidator shadowmap.vert.glsl -V -o shadowmap.vert.spv
glslangValidator shadowmap.frag.glsl -V -o shadowmap.frag.spv
glslangValidator wire_obj.vert.glsl -V -o wire_obj.vert.spv
glslangValidator wire_obj.frag.glsl -V -o wire_obj.frag.spv
move *.spv ../../EEData/Shaders