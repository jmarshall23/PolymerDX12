// interaction_fragment.glsl
//

#ifdef NORMAL_MAP
uniform sampler2D normalMap;
uniform vec2 normalBias;
varying vec3 tangentSpaceEyeVec;
#endif

#ifdef ART_MAP
uniform sampler2D artMap;
uniform sampler2D basePalMap;
uniform sampler2DRect lookupMap;
uniform float shadeOffset;
uniform float visibility;
varying vec3 horizDistance;
#endif

#ifdef DIFFUSE_MAP
uniform sampler2D diffuseMap;
#endif

#ifdef DIFFUSE_DETAIL_MAP
uniform sampler2D detailMap;
varying vec2 fragDetailScale;
#endif

#ifdef HIGHPALOOKUP_MAP
uniform sampler3D highPalookupMap;
#endif

#ifdef SPECULAR_MAP
uniform sampler2D specMap;
#endif

#ifdef SPECULAR_MATERIAL
uniform vec2 specMaterial;
#endif

#ifdef MIRROR_MAP
uniform sampler2DRect mirrorMap;
#endif

#ifdef LINEAR_FOG
uniform bool linearFog;
#endif

#ifdef GLOW_MAP
uniform sampler2D glowMap;
#endif

#ifdef SHADOW_MAP
uniform sampler2DShadow shadowMap;
#endif

#ifdef LIGHT_MAP
uniform sampler2D lightMap;
#endif

#ifdef SPOT_LIGHT
uniform vec3 spotDir;
uniform vec2 spotRadius;
#endif

#ifdef POINT_LIGHT
varying vec3 vertexNormal;
varying vec3 eyeVector;
varying vec3 lightVector;
varying vec3 tangentSpaceLightVector;
#endif

void main(void)
{
	vec3 commonTexCoord = vec3(gl_TexCoord[0].st, 0.0);
	vec4 result = vec4(1.0, 1.0, 1.0, 1.0);
	vec4 diffuseTexel = vec4(1.0, 1.0, 1.0, 1.0);
	vec4 specTexel = vec4(1.0, 1.0, 1.0, 1.0);
	vec4 normalTexel;
	int isLightingPass = 0;
	int isNormalMapped = 0;
	int isSpecularMapped = 0;
	vec3 eyeVec;
	int isSpotLight = 0;
	vec3 spotVector;
	vec2 spotCosRadius;
	float shadowResult = 1.0;
	vec2 specularMaterial = vec2(15.0, 1.0);
	vec3 lightTexel = vec3(1.0, 1.0, 1.0);

	// Shader Body goes in here!
#ifdef LIGHTING_PASS
	isLightingPass = 1;
	result = vec4(0.0, 0.0, 0.0, 1.0);
#endif

#ifdef NORMAL_MAP
	vec4 normalStep;
	float biasedHeight;

	eyeVec = normalize(tangentSpaceEyeVec);

	for (int i = 0; i < 4; i++) {
		normalStep = texture2D(normalMap, commonTexCoord.st);
		biasedHeight = normalStep.a * normalBias.x - normalBias.y;
		commonTexCoord += (biasedHeight - commonTexCoord.z) * normalStep.z * eyeVec;
	}

	normalTexel = texture2D(normalMap, commonTexCoord.st);

	isNormalMapped = 1;
#endif

#ifdef ART_MAP
	float shadeLookup = length(horizDistance) / 1.07 * visibility;
	shadeLookup = shadeLookup + shadeOffset;

	float colorIndex = texture2D(artMap, commonTexCoord.st).r * 256.0;
	float colorIndexNear = texture2DRect(lookupMap, vec2(colorIndex, floor(shadeLookup))).r;
	float colorIndexFar = texture2DRect(lookupMap, vec2(colorIndex, floor(shadeLookup + 1.0))).r;
	float colorIndexFullbright = texture2DRect(lookupMap, vec2(colorIndex, 0.0)).r;

	vec3 texelNear = texture2D(basePalMap, vec2(colorIndexNear, 0.5)).rgb;
	vec3 texelFar = texture2D(basePalMap, vec2(colorIndexFar, 0.5)).rgb;
	diffuseTexel.rgb = texture2D(basePalMap, vec2(colorIndexFullbright, 0.5)).rgb;

	if (isLightingPass == 0) {
		result.rgb = mix(texelNear, texelFar, fract(shadeLookup));
		result.a = 1.0;
		if (colorIndex == 256.0)
			result.a = 0.0;
	}
#endif

#ifdef DIFFUSE_MAP
	diffuseTexel = texture2D(diffuseMap, commonTexCoord.st);
#endif

#ifdef DIFFUSE_DETAIL_MAP
	if (isNormalMapped == 0)
		diffuseTexel *= texture2D(detailMap, gl_TexCoord[1].st);
	else
		diffuseTexel *= texture2D(detailMap, commonTexCoord.st * fragDetailScale);

	diffuseTexel.rgb *= 2.0;
#endif

#ifdef DIFFUSE_MODULATION
	if (isLightingPass == 0)
		result *= vec4(gl_Color);
#endif

#ifdef DIFFUSE_MAP2
	if (isLightingPass == 0)
		result *= diffuseTexel;
#endif

#ifdef HIGHPALOOKUP_MAP
	float highPalScale = 0.9921875; // for 6 bits
	float highPalBias = 0.00390625;
	if (isLightingPass == 0)
		result.rgb = texture3D(highPalookupMap, result.rgb * highPalScale + highPalBias).rgb;

	diffuseTexel.rgb = texture3D(highPalookupMap, diffuseTexel.rgb * highPalScale + highPalBias).rgb;
#endif

#ifdef SPECULAR_MAP
	specTexel = texture2D(specMap, commonTexCoord.st);
	isSpecularMapped = 1;
#endif

#ifdef SPECULAR_MATERIAL
	specularMaterial = specMaterial;
#endif

#ifdef MIRROR_MAP
	vec4 mirrorTexel;
	vec2 mirrorCoords;

	mirrorCoords = gl_FragCoord.st;
	if (isNormalMapped == 1) {
		mirrorCoords += 100.0 * (normalTexel.rg - 0.5);

		mirrorTexel = texture2DRect(mirrorMap, mirrorCoords);
		result = vec4((result.rgb * (1.0 - specTexel.a)) + (mirrorTexel.rgb * specTexel.rgb * specTexel.a), result.a);
	}
#endif

#ifdef FOG
		float fragDepth;
		float fogFactor;

		fragDepth = gl_FragCoord.z / gl_FragCoord.w;
#ifdef LINEAR_FOG
		if (!linearFog) {
#endif
			fragDepth *= fragDepth;
			fogFactor = exp2(-gl_Fog.density * gl_Fog.density * fragDepth * 1.442695);
#ifdef LINEAR_FOG
		}
		else {
			fogFactor = gl_Fog.scale * (gl_Fog.end - fragDepth);
			fogFactor = clamp(fogFactor, 0.0, 1.0);
		}

		result.rgb = mix(gl_Fog.color.rgb, result.rgb, fogFactor);
#endif

#endif

#ifdef GLOW_MAP
		vec4 glowTexel;
		glowTexel = texture2D(glowMap, commonTexCoord.st);
		result = vec4((result.rgb * (1.0 - glowTexel.a)) + (glowTexel.rgb * glowTexel.a), result.a);
#endif

#ifdef SHADOW_MAP
		shadowResult = shadow2DProj(shadowMap, gl_TexCoord[2]).a;
#endif

#ifdef LIGHT_MAP
		lightTexel = texture2D(lightMap, vec2(gl_TexCoord[2].s, -gl_TexCoord[2].t) / gl_TexCoord[2].q).rgb;
#endif

#ifdef SPOT_LIGHT
		spotVector = spotDir;
		spotCosRadius = spotRadius;
		isSpotLight = 1;
#endif

#ifdef POINT_LIGHT
		float pointLightDistance;
		float lightAttenuation;
		float spotAttenuation;
		vec3 N, L, E, R, D;
		vec3 lightDiffuse;
		float lightSpecular;
		float NdotL;
		float spotCosAngle;

		L = normalize(lightVector);

		pointLightDistance = dot(lightVector, lightVector);
		lightAttenuation = clamp(1.0 - pointLightDistance * gl_LightSource[0].linearAttenuation, 0.0, 1.0);
		spotAttenuation = 1.0;

		if (isSpotLight == 1) {
			D = normalize(spotVector);
			spotCosAngle = dot(-L, D);
			spotAttenuation = clamp((spotCosAngle - spotCosRadius.x) * spotCosRadius.y, 0.0, 1.0);
		}

		if (isNormalMapped == 1) {
			E = eyeVec;
			N = normalize(2.0 * (normalTexel.rgb - 0.5));
			L = normalize(tangentSpaceLightVector);
		}
		else {
			E = normalize(eyeVector);
			N = normalize(vertexNormal);
		}
		NdotL = max(dot(N, L), 0.0);

		R = reflect(-L, N);

		lightDiffuse = gl_Color.a * shadowResult * lightTexel *
			gl_LightSource[0].diffuse.rgb * lightAttenuation * spotAttenuation;
		result += vec4(lightDiffuse * diffuseTexel.a * diffuseTexel.rgb * NdotL, 0.0);

		if (isSpecularMapped == 0)
			specTexel.rgb = diffuseTexel.rgb * diffuseTexel.a;

		lightSpecular = pow(max(dot(R, E), 0.0), specularMaterial.x * specTexel.a) * specularMaterial.y;
		result += vec4(lightDiffuse * specTexel.rgb * lightSpecular, 0.0);
#endif
		
		gl_FragColor = result;
	}