@echo off
cd /D "%~dp0"
glslangValidator standard.glsl -DFRAGMENT -DEFT -V -S frag -o standard.frag.spv 
glslangValidator standard.glsl -DVERTEX -V -S vert -o standard.vert.spv 
glslangValidator tonemap.comp.glsl -V -o tonemap.comp.spv
glslangValidator tonemap2d.comp.glsl -V -o tonemap2d.comp.spv
glslangValidator clear_pick_buf.comp.glsl -V -o clear_pick_buf.comp.spv
glslangValidator shadowmap.vert.glsl -V -o shadowmap.vert.spv
glslangValidator blank.frag.glsl -V -o blank.frag.spv
glslangValidator wire_obj.vert.glsl -V -o wire_obj.vert.spv
glslangValidator wire_obj.frag.glsl -V -o wire_obj.frag.spv
glslangValidator skybox.vert.glsl -V -o skybox.vert.spv
glslangValidator skybox.frag.glsl -V -o skybox.frag.spv
glslangValidator full_tri.vert.glsl -V -o full_tri.vert.spv
glslangValidator brdf_lut.frag.glsl -V -o brdf_lut.frag.spv
glslangValidator cubemap_prefilter.comp.glsl -V -o cubemap_prefilter.comp.spv
glslangValidator line.frag.glsl -V -o line.frag.spv
glslangValidator line.vert.glsl -V -o line.vert.spv
glslangValidator depth_prepass.vert.glsl -V -o depth_prepass.vert.spv
glslangValidator alpha_test_prepass.frag.glsl -V -o alpha_test_prepass.frag.spv
glslangValidator standard.glsl -DFRAGMENT -V -S frag -o standard_alpha_test.frag.spv

REM glslangValidator standard.glsl -DFRAGMENT -DAMD_VIEWINDEX_WORKAROUND -V -S frag -o standard_vi_workaround.frag.spv 
REM glslangValidator standard.glsl -DVERTEX -DAMD_VIEWINDEX_WORKAROUND -V -S vert -o standard_vi_workaround.vert.spv 
REM glslangValidator skybox_vi_workaround.vert.glsl -V -o skybox_vi_workaround.vert.spv
move *.spv ../../EEData/Shaders