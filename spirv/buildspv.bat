@echo off
echo Building Vertex Shader
glslc -fentry-point=VSMain -fshader-stage=vertex ../../renderprogs/d3d12/buildshader.hlsl -o ../../renderprogs/spirv/buildshader_v.spv
echo Building Fragment Shader
glslc -fentry-point=PSMain -fshader-stage=fragment ../../renderprogs/d3d12/buildshader.hlsl -o ../../renderprogs/spirv/buildshader_p.spv
