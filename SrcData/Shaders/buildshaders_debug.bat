@echo off
cd /D "%~dp0"
glslangValidator -g standard.glsl -DFRAGMENT -V -S frag -o standard.frag.spv 
glslangValidator -g standard.glsl -DVERTEX -V -S vert -o standard.vert.spv 
glslangValidator -g tonemap.comp.glsl -V -o tonemap.comp.spv
glslangValidator -g tonemap2d.comp.glsl -V -o tonemap2d.comp.spv
glslangValidator -g clear_pick_buf.comp.glsl -V -o clear_pick_buf.comp.spv
glslangValidator -g shadowmap.vert.glsl -V -o shadowmap.vert.spv
glslangValidator -g shadowmap.frag.glsl -V -o shadowmap.frag.spv
glslangValidator -g wire_obj.vert.glsl -V -o wire_obj.vert.spv
glslangValidator -g wire_obj.frag.glsl -V -o wire_obj.frag.spv
glslangValidator -g skybox.vert.glsl -V -o skybox.vert.spv
glslangValidator -g skybox.frag.glsl -V -o skybox.frag.spv
glslangValidator -g full_tri.vert.glsl -V -o full_tri.vert.spv
glslangValidator -g brdf_lut.frag.glsl -V -o brdf_lut.frag.spv
glslangValidator -g cubemap_prefilter.comp.glsl -V -o cubemap_prefilter.comp.spv
glslangValidator -g line.frag.glsl -V -o line.frag.spv
glslangValidator -g line.vert.glsl -V -o line.vert.spv
move *.spv ../../EEData/Shaders