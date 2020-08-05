@echo off
cd /D "%~dp0"
glslangValidator standard.glsl -DFRAGMENT -V -S frag -o standard.frag.spv 
glslangValidator standard.glsl -DVERTEX -V -S vert -o standard.vert.spv 
REM glslangValidator tonemap.comp.glsl -V -o tonemap.comp.spv
REM glslangValidator tonemap2d.comp.glsl -V -o tonemap2d.comp.spv
REM glslangValidator clear_pick_buf.comp.glsl -V -o clear_pick_buf.comp.spv
REM glslangValidator shadowmap.vert.glsl -V -o shadowmap.vert.spv
REM glslangValidator shadowmap.frag.glsl -V -o shadowmap.frag.spv
REM glslangValidator wire_obj.vert.glsl -V -o wire_obj.vert.spv
REM glslangValidator wire_obj.frag.glsl -V -o wire_obj.frag.spv
move *.spv ../../EEData/Shaders