@echo off
cd /D "%~dp0"
glslangValidator standard.glsl -DFRAGMENT -V -S frag -o standard.frag.spv 
glslangValidator standard.glsl -DVERTEX -V -S vert -o standard.vert.spv 
glslangValidator tonemap.comp.glsl -V -o tonemap.comp.spv
glslangValidator tonemap2d.comp.glsl -V -o tonemap2d.comp.spv
glslangValidator clear_pick_buf.comp.glsl -V -o clear_pick_buf.comp.spv
glslangValidator shadowmap.vert.glsl -V -o shadowmap.vert.spv
glslangValidator shadowmap.frag.glsl -V -o shadowmap.frag.spv
glslangValidator wire_obj.vert.glsl -V -o wire_obj.vert.spv
glslangValidator wire_obj.frag.glsl -V -o wire_obj.frag.spv
glslangValidator skybox.vert.glsl -V -o skybox.vert.spv
glslangValidator skybox.frag.glsl -V -o skybox.frag.spv
move *.spv ../../EEData/Shaders