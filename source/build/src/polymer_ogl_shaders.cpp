// polymer_shaders_ogl.cpp
//

#include "compat.h"

#include "polymer.h"
#include "engine_priv.h"
#include "xxhash.h"
#include "texcache.h"
#include "cache1d.h"
#include "buildrender.h"

#include <vector>
#include <string>

const char *glVertexProgramBuffer   = nullptr;
const char *glFragmentProgramBuffer = nullptr;

extern _prprograminfo  prprograms[1 << PR_BIT_COUNT];
extern tr_renderer* m_renderer;
extern tr_cmd* graphicscmd;
extern uint32_t currentShaderProgramID;

struct buildShader_t {
	tr_shader_program* m_shader;	
#ifdef BUILD_VULKAN
	const char* vkMonolithShaderBufferVertex = nullptr;
	int vkMonolithShaderBufferVertexSize = -1;
	const char* vkMonolithShaderBufferFragment = nullptr;
	int vkMonolithShaderBufferFragmentSize = -1;
#else
	const char* d3d12MonolithShaderBuffer = nullptr;
#endif

	void Reset(void) {
		if (vkMonolithShaderBufferVertex) {
			free((void*)vkMonolithShaderBufferVertex);
			vkMonolithShaderBufferVertex = nullptr;
		}
		if (vkMonolithShaderBufferFragment) {
			free((void*)vkMonolithShaderBufferFragment);
			vkMonolithShaderBufferFragment = nullptr;
		}
	}
};

buildShader_t polymerMonolithicShader;
buildShader_t polymerMonolithicTransShader;

#define POLYMER_DX12_MAXDRAWCALLS				3000

enum polymerDescriptorSetType_t {
	DX12_DESCRIPTORSET_BACKFACE = 0,
	DX12_DESCRIPTORSET_FRONTFACE,
	DX12_DESCRIPTORSET_UI,
	DX12_DESCRIPTORSET_TRANS,
	DX12_DESCRIPTORSET_MODELVB_BACKFACE,
	DX12_DESCRIPTORSET_MODELVB_FRONTFACE,
	DX12_DESCRIPTORSET_NUMTYPES
};

tr_descriptor_set* m_desc_set[DX12_DESCRIPTORSET_NUMTYPES][POLYMER_DX12_MAXDRAWCALLS];
tr_pipeline*	   m_pipeline[DX12_DESCRIPTORSET_NUMTYPES][POLYMER_DX12_MAXDRAWCALLS];
tr_buffer*		   m_uniform_buffers[DX12_DESCRIPTORSET_NUMTYPES][POLYMER_DX12_MAXDRAWCALLS];
int numFrameDrawCalls = 0;
int numFrameFlipedWindingDrawCalls = 0;
int numFrameTransDrawCalls = 0;
int numFrameUIDrawCalls = 0;
bool polymer_isRenderingModels = false;

/*
=====================
GL_BindTexture
=====================
*/
void GL_BindTexture(struct tr_texture *texture, int tmu, bool trans, bool ui) {
	extern tr_sampler* m_linear_sampler;
	extern tr_texture* basePaletteTexture;
	extern tr_texture* lookupPaletteTexture;

	polymerDescriptorSetType_t descriptorType = DX12_DESCRIPTORSET_BACKFACE;

	int drawCall = -1;
	if(ui)
	{
		drawCall = numFrameUIDrawCalls;
		descriptorType = DX12_DESCRIPTORSET_UI;
	}
	else
	{
		if (trans) {
			drawCall = numFrameTransDrawCalls;
			descriptorType = DX12_DESCRIPTORSET_TRANS;
		}
		else {
			if (inpreparemirror) {
				if(!polymer_isRenderingModels)
					descriptorType = DX12_DESCRIPTORSET_FRONTFACE;
				else
					descriptorType = DX12_DESCRIPTORSET_MODELVB_FRONTFACE;

				drawCall = numFrameFlipedWindingDrawCalls;
			}
			else {
				if (!polymer_isRenderingModels)
					descriptorType = DX12_DESCRIPTORSET_BACKFACE;
				else
					descriptorType = DX12_DESCRIPTORSET_MODELVB_BACKFACE;
				drawCall = numFrameDrawCalls;
			}
		}
	}

	if (texture == NULL) {
		texture = GetTileSheet(0);
	}

	if (m_desc_set[descriptorType][drawCall]->descriptors[1].textures[0] != NULL && texture != NULL)
	{
		if (m_desc_set[descriptorType][drawCall]->descriptors[1].textures[0] == texture)
			return;
	}

	m_desc_set[descriptorType][drawCall]->descriptors[1].textures[0] = texture;
	m_desc_set[descriptorType][drawCall]->descriptors[2].textures[0] = basePaletteTexture;
	m_desc_set[descriptorType][drawCall]->descriptors[3].textures[0] = lookupPaletteTexture;
	tr_update_descriptor_set(m_renderer, m_desc_set[descriptorType][drawCall]);
}
/*
=====================
GL_BindDescSetForDrawCall
=====================
*/
void GL_BindDescSetForDrawCall(shaderUniformBuffer_t& uniformBuffer, bool depth, bool trans) {
	int drawCall = -1;
	if(!depth) {
		drawCall = numFrameUIDrawCalls;

		memcpy(m_uniform_buffers[DX12_DESCRIPTORSET_UI][drawCall]->cpu_mapped_address, &uniformBuffer, sizeof(shaderUniformBuffer_t));
		tr_cmd_bind_pipeline(graphicscmd, m_pipeline[DX12_DESCRIPTORSET_UI][drawCall]);
		tr_cmd_bind_descriptor_sets(graphicscmd, m_pipeline[DX12_DESCRIPTORSET_UI][drawCall], m_desc_set[DX12_DESCRIPTORSET_UI][drawCall]);
		numFrameUIDrawCalls++;
	}
	else {		
		if (trans) {
			drawCall = numFrameTransDrawCalls;
		}
		else {
			if (inpreparemirror)
				drawCall = numFrameFlipedWindingDrawCalls;
			else
				drawCall = numFrameDrawCalls;
		}

		if (trans)
		{
			memcpy(m_uniform_buffers[DX12_DESCRIPTORSET_TRANS][drawCall]->cpu_mapped_address, &uniformBuffer, sizeof(shaderUniformBuffer_t));
			tr_cmd_bind_pipeline(graphicscmd, m_pipeline[DX12_DESCRIPTORSET_TRANS][drawCall]);
			tr_cmd_bind_descriptor_sets(graphicscmd, m_pipeline[DX12_DESCRIPTORSET_TRANS][drawCall], m_desc_set[DX12_DESCRIPTORSET_TRANS][drawCall]);
		}
		else if (inpreparemirror)
		{		
			if (!polymer_isRenderingModels)
			{
				memcpy(m_uniform_buffers[DX12_DESCRIPTORSET_FRONTFACE][drawCall]->cpu_mapped_address, &uniformBuffer, sizeof(shaderUniformBuffer_t));
				tr_cmd_bind_pipeline(graphicscmd, m_pipeline[DX12_DESCRIPTORSET_FRONTFACE][drawCall]);
				tr_cmd_bind_descriptor_sets(graphicscmd, m_pipeline[DX12_DESCRIPTORSET_FRONTFACE][drawCall], m_desc_set[DX12_DESCRIPTORSET_FRONTFACE][drawCall]);
			}
			else
			{
				memcpy(m_uniform_buffers[DX12_DESCRIPTORSET_MODELVB_FRONTFACE][drawCall]->cpu_mapped_address, &uniformBuffer, sizeof(shaderUniformBuffer_t));
				tr_cmd_bind_pipeline(graphicscmd, m_pipeline[DX12_DESCRIPTORSET_MODELVB_FRONTFACE][drawCall]);
				tr_cmd_bind_descriptor_sets(graphicscmd, m_pipeline[DX12_DESCRIPTORSET_MODELVB_FRONTFACE][drawCall], m_desc_set[DX12_DESCRIPTORSET_MODELVB_FRONTFACE][drawCall]);
			}
		}
		else
		{
			if (!polymer_isRenderingModels)
			{
				memcpy(m_uniform_buffers[DX12_DESCRIPTORSET_BACKFACE][drawCall]->cpu_mapped_address, &uniformBuffer, sizeof(shaderUniformBuffer_t));
				tr_cmd_bind_pipeline(graphicscmd, m_pipeline[DX12_DESCRIPTORSET_BACKFACE][drawCall]);
				tr_cmd_bind_descriptor_sets(graphicscmd, m_pipeline[DX12_DESCRIPTORSET_BACKFACE][drawCall], m_desc_set[DX12_DESCRIPTORSET_BACKFACE][drawCall]);
			}
			else
			{
				memcpy(m_uniform_buffers[DX12_DESCRIPTORSET_MODELVB_BACKFACE][drawCall]->cpu_mapped_address, &uniformBuffer, sizeof(shaderUniformBuffer_t));
				tr_cmd_bind_pipeline(graphicscmd, m_pipeline[DX12_DESCRIPTORSET_MODELVB_BACKFACE][drawCall]);
				tr_cmd_bind_descriptor_sets(graphicscmd, m_pipeline[DX12_DESCRIPTORSET_MODELVB_BACKFACE][drawCall], m_desc_set[DX12_DESCRIPTORSET_MODELVB_BACKFACE][drawCall]);
			}
		}

		if (!trans) {
			if (!inpreparemirror)
				numFrameDrawCalls++;
			else
				numFrameFlipedWindingDrawCalls++;
		}
		else {
			numFrameTransDrawCalls++;
		}		
	}		
}

/*
=====================
polymost_useShaderProgram
=====================
*/
void polymost_useShaderProgram(uint32_t shaderID) {
	if (rhiType == RHI_OPENGL) {
		glUseProgram(shaderID);
	}
	else if (rhiType == RHI_D3D12) {
		// Do Nothing.
	}
	else {
		assert(!"Unknown rhi type!");
	}
	currentShaderProgramID = shaderID;
}

/*
=====================
polymer_loadmonoshader
=====================
*/
int polymer_loadmonoshader(const char *macros) {
	std::string fullShader;

	if(macros) {
		fullShader += macros;
	}

#ifdef BUILD_VULKAN
	tr_create_shader_program(m_renderer, polymerMonolithicShader.vkMonolithShaderBufferVertexSize, polymerMonolithicShader.vkMonolithShaderBufferVertex, "VSMain", polymerMonolithicShader.vkMonolithShaderBufferFragmentSize, polymerMonolithicShader.vkMonolithShaderBufferFragment, "PSMain", &polymerMonolithicShader.m_shader);
	tr_create_shader_program(m_renderer, polymerMonolithicTransShader.vkMonolithShaderBufferVertexSize, polymerMonolithicTransShader.vkMonolithShaderBufferVertex, "VSMain", polymerMonolithicTransShader.vkMonolithShaderBufferFragmentSize, polymerMonolithicTransShader.vkMonolithShaderBufferFragment, "PSMain", &polymerMonolithicTransShader.m_shader);
#else
	fullShader += d3d12MonolithShaderBuffer;
	tr_create_shader_program(m_renderer, (uint32_t)fullShader.size(), (uint32_t*)(fullShader.data()), "VSMain", (uint32_t)fullShader.size(), (uint32_t*)(fullShader.data()), "PSMain", &newShader.m_shader);
#endif

	if(polymerMonolithicTransShader.m_shader == NULL || polymerMonolithicShader.m_shader == NULL) {
		return -1;
	}

	for (int d = 0; d < DX12_DESCRIPTORSET_NUMTYPES; d++)
	{
		int32_t binding = 0;

		if (d == DX12_DESCRIPTORSET_MODELVB_BACKFACE || d == DX12_DESCRIPTORSET_MODELVB_FRONTFACE) {
			binding = 1;
		}

		tr_vertex_layout vertex_layout = {};
		vertex_layout.attrib_count = 4;
		vertex_layout.attribs[0].semantic = tr_semantic_position;
		vertex_layout.attribs[0].format = tr_format_r32g32b32_float;
		vertex_layout.attribs[0].binding = binding;
		vertex_layout.attribs[0].location = 0;
		vertex_layout.attribs[0].offset = 0;
		vertex_layout.attribs[1].semantic = tr_semantic_texcoord0;
		vertex_layout.attribs[1].format = tr_format_r32g32_float;
		vertex_layout.attribs[1].binding = binding;
		vertex_layout.attribs[1].location = 1;
		vertex_layout.attribs[1].offset = tr_util_format_stride(vertex_layout.attribs[0].format);
		vertex_layout.attribs[2].semantic = tr_semantic_color;
		vertex_layout.attribs[2].format = tr_format_r32g32b32a32_float;
		vertex_layout.attribs[2].binding = binding;
		vertex_layout.attribs[2].location = 2;
		vertex_layout.attribs[2].offset = vertex_layout.attribs[1].offset + tr_util_format_stride(vertex_layout.attribs[1].format);
		vertex_layout.attribs[3].semantic = tr_semantic_texcoord1;
		vertex_layout.attribs[3].format = tr_format_r32g32b32_float;
		vertex_layout.attribs[3].binding = binding;
		vertex_layout.attribs[3].location = 3;
		vertex_layout.attribs[3].offset = vertex_layout.attribs[2].offset + tr_util_format_stride(vertex_layout.attribs[2].format);

		for (int i = 0; i < POLYMER_DX12_MAXDRAWCALLS; i++)
		{
			std::vector<tr_descriptor> descriptors(4);
			descriptors[0].type = tr_descriptor_type_uniform_buffer_cbv;
			descriptors[0].count = 1;
			descriptors[0].binding = 0;
			descriptors[0].shader_stages = tr_shader_stage_vert;

			descriptors[1].type = tr_descriptor_type_texture_srv;
			descriptors[1].count = 1;
			descriptors[1].binding = 1;
			descriptors[1].shader_stages = tr_shader_stage_frag;

			//descriptors[2].type = tr_descriptor_type_sampler;
			//descriptors[2].count = 1;
			//descriptors[2].binding = 2;
			//descriptors[2].shader_stages = tr_shader_stage_frag;

			descriptors[2].type = tr_descriptor_type_texture_srv;
			descriptors[2].count = 1;
			descriptors[2].binding = 3;
			descriptors[2].shader_stages = tr_shader_stage_frag;

			descriptors[3].type = tr_descriptor_type_texture_srv;
			descriptors[3].count = 1;
			descriptors[3].binding = 4;
			descriptors[3].shader_stages = tr_shader_stage_frag;

			tr_create_descriptor_set(m_renderer, (uint32_t)descriptors.size(), descriptors.data(), &m_desc_set[d][i]);

			tr_pipeline_settings pipeline_settings = { tr_primitive_topo_tri_list };

			if (d != DX12_DESCRIPTORSET_UI) {
				pipeline_settings.depth = true;
				if (d == DX12_DESCRIPTORSET_BACKFACE || d == DX12_DESCRIPTORSET_MODELVB_BACKFACE)
				{
					pipeline_settings.cull_mode = tr_cull_mode_back;
				}
				else if (d == DX12_DESCRIPTORSET_FRONTFACE || d == DX12_DESCRIPTORSET_MODELVB_FRONTFACE)
				{
					pipeline_settings.cull_mode = tr_cull_mode_front;
				}

				if (d == DX12_DESCRIPTORSET_TRANS)
				{
					pipeline_settings.alphaBlend = true;
				}
			}
			else {
				pipeline_settings.depth = false;
			}

			if (d == DX12_DESCRIPTORSET_TRANS)
				tr_create_pipeline(m_renderer, polymerMonolithicTransShader.m_shader, &vertex_layout, m_desc_set[d][i], m_renderer->swapchain_render_targets[0], &pipeline_settings, &m_pipeline[d][i]);
			else
				tr_create_pipeline(m_renderer, polymerMonolithicShader.m_shader, &vertex_layout, m_desc_set[d][i], m_renderer->swapchain_render_targets[0], &pipeline_settings, &m_pipeline[d][i]);

			tr_create_uniform_buffer(m_renderer, sizeof(shaderUniformBuffer_t), true, &m_uniform_buffers[d][i]);
			m_desc_set[d][i]->descriptors[0].uniform_buffers[0] = m_uniform_buffers[d][i];
		}
	}

	return 1;
}

/*
===============================
polymer_ogl_loadinteraction
===============================
*/
void polymer_ogl_loadinteraction(void) {
	if (rhiType == RHI_OPENGL) {
		if (glVertexProgramBuffer)
		{
			free((void*)glVertexProgramBuffer);
			glVertexProgramBuffer = nullptr;
		}

		if (glFragmentProgramBuffer)
		{
			free((void*)glFragmentProgramBuffer);
			glFragmentProgramBuffer = nullptr;
		}

		kpzbufload2("renderprogs/glsl/interaction_vertex.glsl", (char**)&glVertexProgramBuffer);
		kpzbufload2("renderprogs/glsl/interaction_fragment.glsl", (char**)&glFragmentProgramBuffer);
	}
	else if(rhiType == RHI_D3D12) {
#ifdef BUILD_VULKAN
		polymerMonolithicShader.Reset();
		polymerMonolithicShader.vkMonolithShaderBufferVertexSize = kpzbufload2("renderprogs/spirv/buildshader_v.spv", (char**)&polymerMonolithicShader.vkMonolithShaderBufferVertex);
		polymerMonolithicShader.vkMonolithShaderBufferFragmentSize = kpzbufload2("renderprogs/spirv/buildshader_p.spv", (char**)&polymerMonolithicShader.vkMonolithShaderBufferFragment);

		polymerMonolithicTransShader.Reset();
		polymerMonolithicTransShader.vkMonolithShaderBufferVertexSize = kpzbufload2("renderprogs/spirv/buildshader_trans33_v.spv", (char**)&polymerMonolithicTransShader.vkMonolithShaderBufferVertex);
		polymerMonolithicTransShader.vkMonolithShaderBufferFragmentSize = kpzbufload2("renderprogs/spirv/buildshader_trans33_p.spv", (char**)&polymerMonolithicTransShader.vkMonolithShaderBufferFragment);
#else
		if (d3d12MonolithShaderBuffer) {
			free((void*)d3d12MonolithShaderBuffer);
			d3d12MonolithShaderBuffer = nullptr;
		}

		kpzbufload2("renderprogs/d3d12/buildshader.hlsl", (char**)&d3d12MonolithShaderBuffer);
#endif
		polymer_loadmonoshader(NULL);
	}
	else {
		assert(!"Unknown RHI type");
	}
}

/*
===============================
polymer_ogl_compileprogram
===============================
*/
void  polymer_ogl_compileprogram(int32_t programbits)
{
	// In Direct3D 12 we don't have a shader per material.
	if(rhiType == RHI_OPENGL) {
		int32_t         i, enabledbits;
		GLuint          vert, frag, program;
		const GLchar* source[PR_BIT_COUNT * 2];
		GLchar       infobuffer[PR_INFO_LOG_BUFFER_SIZE];
		GLint           linkstatus;

		// --------- VERTEX
		vert = glCreateShader(GL_VERTEX_SHADER);

		enabledbits = i = 0;
		while (i < PR_BIT_COUNT)
		{
			if (programbits & prprogrambits[i].bit)
				source[enabledbits++] = prprogrambits[i].programMacros;
			i++;
		}
		i = 0;

		source[enabledbits++] = glVertexProgramBuffer;


		glShaderSource(vert, enabledbits, source, NULL);

		glCompileShader(vert);

		// --------- FRAGMENT
		frag = glCreateShader(GL_FRAGMENT_SHADER);

		enabledbits = i = 0;
		while (i < PR_BIT_COUNT)
		{
			if (programbits & prprogrambits[i].bit)
				source[enabledbits++] = prprogrambits[i].programMacros;
			i++;
		}
		i = 0;
		source[enabledbits++] = glFragmentProgramBuffer;

		glShaderSource(frag, enabledbits, (const GLchar**)source, NULL);

		glCompileShader(frag);

		// --------- PROGRAM
		program = glCreateProgram();

		glAttachShader(program, vert);
		glAttachShader(program, frag);

		glLinkProgram(program);

		glGetProgramiv(program, GL_LINK_STATUS, &linkstatus);

		glGetProgramInfoLog(program, PR_INFO_LOG_BUFFER_SIZE, NULL, infobuffer);

		prprograms[programbits].handle = program;

#ifdef DEBUGGINGAIDS
		if (pr_verbosity >= 1)
#else
		if (pr_verbosity >= 2)
#endif
			OSD_Printf("PR : Compiling GPU program with bits (octal) %o...\n", (unsigned)programbits);
		if (!linkstatus) {
			OSD_Printf("PR : Failed to compile GPU program with bits (octal) %o!\n", (unsigned)programbits);
			if (pr_verbosity >= 1) OSD_Printf("PR : Compilation log:\n%s\n", infobuffer);
			glGetShaderSource(vert, PR_INFO_LOG_BUFFER_SIZE, NULL, infobuffer);
			if (pr_verbosity >= 1) OSD_Printf("PR : Vertex source dump:\n%s\n", infobuffer);
			glGetShaderSource(frag, PR_INFO_LOG_BUFFER_SIZE, NULL, infobuffer);
			if (pr_verbosity >= 1) OSD_Printf("PR : Fragment source dump:\n%s\n", infobuffer);
		}

		// --------- ATTRIBUTE/UNIFORM LOCATIONS

		// PR_BIT_ANIM_INTERPOLATION
		if (programbits & prprogrambits[PR_BIT_ANIM_INTERPOLATION].bit)
		{
			prprograms[programbits].attrib_nextFrameData = glGetAttribLocation(program, "nextFrameData");
			prprograms[programbits].attrib_nextFrameNormal = glGetAttribLocation(program, "nextFrameNormal");
			prprograms[programbits].uniform_frameProgress = glGetUniformLocation(program, "frameProgress");
		}

		// PR_BIT_NORMAL_MAP
		if (programbits & prprogrambits[PR_BIT_NORMAL_MAP].bit)
		{
			prprograms[programbits].attrib_T = glGetAttribLocation(program, "T");
			prprograms[programbits].attrib_B = glGetAttribLocation(program, "B");
			prprograms[programbits].attrib_N = glGetAttribLocation(program, "N");
			prprograms[programbits].uniform_eyePosition = glGetUniformLocation(program, "eyePosition");
			prprograms[programbits].uniform_normalMap = glGetUniformLocation(program, "normalMap");
			prprograms[programbits].uniform_normalBias = glGetUniformLocation(program, "normalBias");
		}

		// PR_BIT_ART_MAP
		if (programbits & prprogrambits[PR_BIT_ART_MAP].bit)
		{
			prprograms[programbits].uniform_artMap = glGetUniformLocation(program, "artMap");
			prprograms[programbits].uniform_basePalMap = glGetUniformLocation(program, "basePalMap");
			prprograms[programbits].uniform_lookupMap = glGetUniformLocation(program, "lookupMap");
			prprograms[programbits].uniform_shadeOffset = glGetUniformLocation(program, "shadeOffset");
			prprograms[programbits].uniform_visibility = glGetUniformLocation(program, "visibility");
		}

		// PR_BIT_DIFFUSE_MAP
		if (programbits & prprogrambits[PR_BIT_DIFFUSE_MAP].bit)
		{
			prprograms[programbits].uniform_diffuseMap = glGetUniformLocation(program, "diffuseMap");
			prprograms[programbits].uniform_diffuseScale = glGetUniformLocation(program, "diffuseScale");
		}

		// PR_BIT_HIGHPALOOKUP_MAP
		if (programbits & prprogrambits[PR_BIT_HIGHPALOOKUP_MAP].bit)
		{
			prprograms[programbits].uniform_highPalookupMap = glGetUniformLocation(program, "highPalookupMap");
		}

		// PR_BIT_DIFFUSE_DETAIL_MAP
		if (programbits & prprogrambits[PR_BIT_DIFFUSE_DETAIL_MAP].bit)
		{
			prprograms[programbits].uniform_detailMap = glGetUniformLocation(program, "detailMap");
			prprograms[programbits].uniform_detailScale = glGetUniformLocation(program, "detailScale");
		}

		// PR_BIT_SPECULAR_MAP
		if (programbits & prprogrambits[PR_BIT_SPECULAR_MAP].bit)
		{
			prprograms[programbits].uniform_specMap = glGetUniformLocation(program, "specMap");
		}

		// PR_BIT_SPECULAR_MATERIAL
		if (programbits & prprogrambits[PR_BIT_SPECULAR_MATERIAL].bit)
		{
			prprograms[programbits].uniform_specMaterial = glGetUniformLocation(program, "specMaterial");
		}

		// PR_BIT_MIRROR_MAP
		if (programbits & prprogrambits[PR_BIT_MIRROR_MAP].bit)
		{
			prprograms[programbits].uniform_mirrorMap = glGetUniformLocation(program, "mirrorMap");
		}
#ifdef PR_LINEAR_FOG
		if (programbits & prprogrambits[PR_BIT_FOG].bit)
		{
			prprograms[programbits].uniform_linearFog = glGetUniformLocation(program, "linearFog");
		}
#endif
		// PR_BIT_GLOW_MAP
		if (programbits & prprogrambits[PR_BIT_GLOW_MAP].bit)
		{
			prprograms[programbits].uniform_glowMap = glGetUniformLocation(program, "glowMap");
		}

		// PR_BIT_PROJECTION_MAP
		if (programbits & prprogrambits[PR_BIT_PROJECTION_MAP].bit)
		{
			prprograms[programbits].uniform_shadowProjMatrix = glGetUniformLocation(program, "shadowProjMatrix");
		}

		// PR_BIT_SHADOW_MAP
		if (programbits & prprogrambits[PR_BIT_SHADOW_MAP].bit)
		{
			prprograms[programbits].uniform_shadowMap = glGetUniformLocation(program, "shadowMap");
		}

		// PR_BIT_LIGHT_MAP
		if (programbits & prprogrambits[PR_BIT_LIGHT_MAP].bit)
		{
			prprograms[programbits].uniform_lightMap = glGetUniformLocation(program, "lightMap");
		}

		// PR_BIT_SPOT_LIGHT
		if (programbits & prprogrambits[PR_BIT_SPOT_LIGHT].bit)
		{
			prprograms[programbits].uniform_spotDir = glGetUniformLocation(program, "spotDir");
			prprograms[programbits].uniform_spotRadius = glGetUniformLocation(program, "spotRadius");
		}
	}
}