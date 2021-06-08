cd /D "%~dp0"
glslangValidator standard.glsl -DFRAGMENT -DEFT -V -S frag -o standard.frag.spv 
glslangValidator standard.glsl -DVERTEX -V -S vert -o standard.vert.spv 
move *.spv ../../EEData/Shaders