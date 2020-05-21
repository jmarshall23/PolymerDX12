glslc -fentry-point=VSMain -fshader-stage=vertex ../renderprogs/d3d12/buildshader.hlsl -o ../renderprogs/spirv/buildshader_v.spv
glslc -fentry-point=PSMain -fshader-stage=fragment ../renderprogs/d3d12/buildshader.hlsl -o ../renderprogs/spirv/buildshader_p.spv
pause