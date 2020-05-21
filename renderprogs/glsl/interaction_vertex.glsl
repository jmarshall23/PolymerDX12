// interaction_vertex.glsl
//

#ifdef ANIM_INTERPOLATION
attribute vec4 nextFrameData;
attribute vec4 nextFrameNormal;
uniform float frameProgress;
#endif

#ifdef NORMAL_MAP
attribute vec3 T;
attribute vec3 B;
attribute vec3 N;
uniform vec3 eyePosition;
varying vec3 tangentSpaceEyeVec;
#endif

#ifdef ART_MAP
varying vec3 horizDistance;
#endif

#ifdef DIFFUSE_MAP
uniform vec2 diffuseScale;
#endif

#ifdef DIFFUSE_DETAIL_MAP
uniform vec2 detailScale;
varying vec2 fragDetailScale;
#endif

#ifdef PROJECTION_MAP
uniform mat4 shadowProjMatrix;
#endif

#ifdef POINT_LIGHT
varying vec3 vertexNormal;
varying vec3 eyeVector;
varying vec3 lightVector;
varying vec3 tangentSpaceLightVector;
#endif

void main(void)
{
	vec4 curVertex = gl_Vertex;
	vec3 curNormal = gl_Normal;
	int isNormalMapped = 0;
	mat3 TBN;

	gl_TexCoord[0] = gl_MultiTexCoord0;

#ifdef ANIM_INTERPOLATION
	vec4 currentFramePosition;
	vec4 nextFramePosition;

	currentFramePosition = curVertex * (1.0 - frameProgress);
	nextFramePosition = nextFrameData * frameProgress;
	curVertex = currentFramePosition + nextFramePosition;

	currentFramePosition = vec4(curNormal, 1.0) * (1.0 - frameProgress);
	nextFramePosition = nextFrameNormal * frameProgress;
	curNormal = vec3(currentFramePosition + nextFramePosition);
#endif

#ifdef NORMAL_MAP
	TBN = mat3(T, B, N);
	tangentSpaceEyeVec = eyePosition - vec3(curVertex);
	tangentSpaceEyeVec = TBN * tangentSpaceEyeVec;

	isNormalMapped = 1;
#endif

#ifdef DIFFUSE_MAP
	gl_TexCoord[0] = vec4(diffuseScale, 1.0, 1.0) * gl_MultiTexCoord0;
#endif

#ifdef DIFFUSE_DETAIL_MAP
	fragDetailScale = detailScale;
	if (isNormalMapped == 0)
		gl_TexCoord[1] = vec4(detailScale, 1.0, 1.0) * gl_MultiTexCoord0;
#endif

#ifdef DIFFUSE_MODULATION
	gl_FrontColor = gl_Color;
#endif

#ifdef PROJECTION_MAP
	gl_TexCoord[2] = shadowProjMatrix * curVertex;
#endif

#ifdef POINT_LIGHT
	vec3 vertexPos;

	vertexPos = vec3(gl_ModelViewMatrix * curVertex);
	eyeVector = -vertexPos;
	lightVector = gl_LightSource[0].ambient.rgb - vertexPos;

	if (isNormalMapped == 1) {
		tangentSpaceLightVector = gl_LightSource[0].specular.rgb - vec3(curVertex);
		tangentSpaceLightVector = TBN * tangentSpaceLightVector;
	}
	else
		vertexNormal = normalize(gl_NormalMatrix * curNormal);

#endif

	gl_Position = gl_ModelViewProjectionMatrix * curVertex;
}
