// gpustates.cpp
//

#include "build.h"
#include "buildrender.h"
#include "engine_priv.h"
#include <vector>
#include <windowsnumerics.h>

extern tr_cmd* graphicscmd;

tr_buffer* prd3d12_vertex_buffer;
tr_buffer* prd3d12_index_buffer[3][MAX_DRAWROOM_LAYERS];
tr_buffer* prd3d12_null_index_buffer;

extern uint32_t frameIdx;

tr_sampler* m_linear_sampler;
extern tr_renderer* m_renderer;

extern int numFrameDrawCalls;
extern int numFrameTransDrawCalls;
extern int numFrameUIDrawCalls;
extern int numSpriteVertexes;
extern int numFrameFlipedWindingDrawCalls;
extern int numSpriteIndxes;

extern bool shouldRenderSky;

float projection_matrix[16] = { 1, 0, 0, 0,
								0, 1, 0, 0,
								0, 0, 1, 0,
								0, 0, 0, 1 };

float modelview_matrix[16] = { 1, 0, 0, 0,
								0, 1, 0, 0,
								0, 0, 1, 0,
								0, 0, 0, 1 };

float identity_matrix[16] = { 1, 0, 0, 0,
								0, 1, 0, 0,
								0, 0, 1, 0,
								0, 0, 0, 1 };

float state_color[4] = { 1, 1, 1, 1 };

void GL_MultiplyMatrix(float * m1Ptr, float * m2Ptr, float * dstPtr) {
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			*dstPtr = m1Ptr[0] * m2Ptr[0 * 4 + j]
				+ m1Ptr[1] * m2Ptr[1 * 4 + j]
				+ m1Ptr[2] * m2Ptr[2 * 4 + j]
				+ m1Ptr[3] * m2Ptr[3 * 4 + j];
			dstPtr++;
		}
		m1Ptr += 4;
	}
}

void GL_Init(void) {
	tr_create_sampler(m_renderer, &m_linear_sampler);

#ifndef BUILD_VULKAN
	m_linear_sampler->dx_sampler_desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	m_linear_sampler->dx_sampler_desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	m_linear_sampler->dx_sampler_desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	m_linear_sampler->dx_sampler_desc.Filter = D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT;
#endif

	uint64_t vertexDataSize = sizeof(Vertex) * POLYMER_DX12_MAXVERTS;
	uint32_t vertexStride = sizeof(Vertex);
	uint64_t indexDataSize = sizeof(uint32_t) * POLYMER_DX12_MAXINDEXES;

	tr_create_vertex_buffer(m_renderer, vertexDataSize, true, vertexStride, &prd3d12_vertex_buffer);
	for (int d = 0; d < 3; d++)
		for (int i = 0; i < MAX_DRAWROOM_LAYERS; i++) {
		{
			tr_create_index_buffer(m_renderer, indexDataSize, true, tr_index_type_uint32, &prd3d12_index_buffer[d][i]);
		}
	}

	tr_create_index_buffer(m_renderer, 6 * sizeof(uint32_t), true, tr_index_type_uint32, &prd3d12_null_index_buffer);
	memset(((unsigned char*)prd3d12_null_index_buffer->cpu_mapped_address), 0, sizeof(uint32_t) * 6);
}

void GL_SetProjectionMatrix(float* newProjectionMatrix) {
	memcpy(projection_matrix, newProjectionMatrix, sizeof(float) * 16);
}

void GL_SetModelViewMatrix(float *newModelViewMatrix) {
	memcpy(modelview_matrix, newModelViewMatrix, sizeof(float) * 16);
}

void GL_SetModelViewToIdentity(void) {
	memcpy(modelview_matrix, identity_matrix, sizeof(float) * 16);
}

void GL_SetColor(float r, float g, float b, float a) {
	state_color[0] = r;
	state_color[1] = g;
	state_color[2] = b;
	state_color[3] = a;
}

void GL_DrawBuffer(int picnum, float * drawpolyVerts, int numPoints) {
	float TileRectInfo[4];

	if (picnum >= 0) {
		polymost_gettileinfo(picnum, TileRectInfo[0], TileRectInfo[1], TileRectInfo[2], TileRectInfo[3]);
	}
	else {
		// This is for .anm files which aren't part of the atlas!
		TileRectInfo[0] = 0;
		TileRectInfo[1] = 0;
		TileRectInfo[2] = 0;
		TileRectInfo[3] = 0;
	}

	int startVertex = POLYMER_DX12_MAXLEVELVERTS + numGuiVertexes;
	uint32_t quadindices[6] = { 0, 1, 2, 3, 2, 0 };

	if(numPoints == 6) {
		for (int d = 0; d < 6; d++)
			quadindices[d] = d;
	}
	else if(numPoints == 4){
		numPoints = 6;
	}
	else {
		return;
	}

	Vertex* cpuVertexPointer = (Vertex*)(((unsigned char*)prd3d12_vertex_buffer->cpu_mapped_address) + (startVertex * sizeof(Vertex)));
	for (bssize_t d = 0; d < 6; d++, cpuVertexPointer++)
	{
		int i = quadindices[d];

		//update verts
		cpuVertexPointer->position[0] = drawpolyVerts[(i) * 5];
		cpuVertexPointer->position[1] = drawpolyVerts[(i) * 5 + 1];
		cpuVertexPointer->position[2] = drawpolyVerts[(i) * 5 + 2];

		//update texcoords
		cpuVertexPointer->st[0] = drawpolyVerts[(i) * 5 + 3];
		cpuVertexPointer->st[1] = drawpolyVerts[(i) * 5 + 4];

		cpuVertexPointer->TileRect[0] = TileRectInfo[0];
		cpuVertexPointer->TileRect[1] = TileRectInfo[1];
		cpuVertexPointer->TileRect[2] = TileRectInfo[2];
		cpuVertexPointer->TileRect[3] = TileRectInfo[3];

		// TODO: Add gui palettes.
		cpuVertexPointer->info[0] = globalshade;
		cpuVertexPointer->info[1] = 0;
		cpuVertexPointer->info[2] = packint(globalpal, curbasepal, 0, 0);;

		//board_vertexes.push_back(v);
		//board_vertexes[POLYMER_DX12_MAXLEVELVERTS + numGuiVertexes] = *cpuVertexPointer;
		numGuiVertexes++;
	}

	shaderUniformBuffer_t uniformBuffer;
	GL_MultiplyMatrix(modelview_matrix, projection_matrix, uniformBuffer.mvp);
	//memcpy(uniformBuffer.worldbuffer, modelview_matrix, sizeof(float) * 16);

	GL_BindDescSetForDrawCall(uniformBuffer, false, false);

	tr_cmd_draw(graphicscmd, numPoints, startVertex);
}


void GL_DrawBuffer(int startIndex, int numIndexes, bool alphaBlend) {
	shaderUniformBuffer_t uniformBuffer;
	GL_MultiplyMatrix(modelview_matrix, projection_matrix, uniformBuffer.mvp);
	//memcpy(uniformBuffer.worldbuffer, modelview_matrix, sizeof(float) * 16);
	GL_BindDescSetForDrawCall(uniformBuffer, true, alphaBlend);

	tr_cmd_draw_indexed(graphicscmd, numIndexes, startIndex);

	//tr_cmd_bind_index_buffer(graphicscmd, prd3d12_null_index_buffer);	
}


void GL_DrawBufferVertex(int startVertex, int numPoints) {
	shaderUniformBuffer_t uniformBuffer;
	GL_MultiplyMatrix(modelview_matrix, projection_matrix, uniformBuffer.mvp);
	//memcpy(uniformBuffer.worldbuffer, modelview_matrix, sizeof(float) * 16);
	GL_BindDescSetForDrawCall(uniformBuffer, true, false);

	tr_cmd_draw(graphicscmd, numPoints, startVertex);

	//tr_cmd_bind_index_buffer(graphicscmd, prd3d12_null_index_buffer);
}


void GL_EndFrame(void) {
	currentDrawRoomLayer = 0;
	numFrameDrawCalls = 0;
	numGuiVertexes = 0;
	numFrameTransDrawCalls = 0;
	numSpriteVertexes = 0;
	numFrameFlipedWindingDrawCalls = 0;
	numFrameUIDrawCalls = 0;
	numSpriteIndxes = 0;
	shouldRenderSky = true;
}