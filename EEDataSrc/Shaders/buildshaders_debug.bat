@echo off
cd /D "%~dp0"
glslangValidator -g standard.frag.glsl -V -o standard.frag.spv
glslangValidator -g standard.vert.glsl -V -o standard.vert.spv
glslangValidator -g tonemap.comp.glsl -V -o tonemap.comp.spv
glslangValidator -g tonemap2d.comp.glsl -V -o tonemap2d.comp.spv
glslangValidator -g clear_pick_buf.comp.glsl -V -o clear_pick_buf.comp.spv
glslangValidator -g shadowmap.vert.glsl -V -o shadowmap.vert.spv
glslangValidator -g shadowmap.frag.glsl -V -o shadowmap.frag.spv
glslangValidator -g wire_obj.vert.glsl -V -o wire_obj.vert.spv
glslangValidator -g wire_obj.frag.glsl -V -o wire_obj.frag.spv
move *.spv ../../EEData/Shaders