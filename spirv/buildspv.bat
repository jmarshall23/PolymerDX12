@echo off
echo Building Vertex Shader
glslc -fentry-point=VSMain -fshader-stage=vertex ../../renderprogs/d3d12/base.hlsl -o ../../renderprogs/spirv/buildshader_v.spv
glslc -fentry-point=VSMain -fshader-stage=vertex ../../renderprogs/d3d12/trans33.hlsl -o ../../renderprogs/spirv/buildshader_trans33_v.spv
echo Building Fragment Shader
glslc -fentry-point=PSMain -fshader-stage=fragment ../../renderprogs/d3d12/base.hlsl -o ../../renderprogs/spirv/buildshader_p.spv
glslc -fentry-point=PSMain -fshader-stage=fragment ../../renderprogs/d3d12/trans33.hlsl -o ../../renderprogs/spirv/buildshader_trans33_p.spv
