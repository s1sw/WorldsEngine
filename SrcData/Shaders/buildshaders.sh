glslc --target-env=vulkan1.2 -fshader-stage=frag -I Include standard.glsl -DFRAGMENT -DEFT  -o standard.frag.spv
glslc --target-env=vulkan1.2 -fshader-stage=vert -I Include standard.glsl -DVERTEX -o standard.vert.spv
glslc --target-env=vulkan1.2 -fshader-stage=frag -I Include standard.glsl -DFRAGMENT -o standard_alpha_test.frag.spv
glslangValidator tonemap.comp.glsl -DMSAA -V -o tonemap.comp.spv
glslangValidator tonemap.comp.glsl -V -o tonemap_nomsaa.comp.spv
glslangValidator clear_pick_buf.comp.glsl -V -o clear_pick_buf.comp.spv
glslangValidator shadowmap.vert.glsl -V -o shadowmap.vert.spv
glslangValidator blank.frag.glsl -V -o blank.frag.spv
glslc --target-env=vulkan1.2 -fshader-stage=vert -I Include wire_obj.vert.glsl -o wire_obj.vert.spv
glslc --target-env=vulkan1.2 -fshader-stage=frag -I Include wire_obj.frag.glsl -o wire_obj.frag.spv
glslangValidator skybox.vert.glsl -V -o skybox.vert.spv
glslangValidator skybox.frag.glsl -V -o skybox.frag.spv
glslangValidator full_tri.vert.glsl -V -o full_tri.vert.spv
glslangValidator brdf_lut.frag.glsl -V -o brdf_lut.frag.spv
glslangValidator cubemap_prefilter.comp.glsl -V -o cubemap_prefilter.comp.spv
glslangValidator line.frag.glsl -V -o line.frag.spv
glslangValidator line.vert.glsl -V -o line.vert.spv
glslc --target-env=vulkan1.2 -fshader-stage=vert -I Include depth_prepass.vert.glsl -o depth_prepass.vert.spv
glslangValidator alpha_test_prepass.frag.glsl -V -o alpha_test_prepass.frag.spv
glslangValidator skin.comp.glsl -V -o skin.comp.spv
glslangValidator gtao.comp.glsl -V -o gtao.comp.spv
glslangValidator blur.comp.glsl -V -o blur.comp.spv
glslangValidator vr_hidden.vert.glsl -V -o vr_hidden.vert.spv

cp blank.frag.spv shadowmap.frag.spv
mv *.spv ../../Data/Shaders
echo 'Shader compilation complete'
