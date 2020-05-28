// buildshader.hlsl
//

//
// Vertex Shader
//

cbuffer UniformBlock0 : register(b0)
{
	float4x4 modelMatrix;
	float4x4 mvp;
};

struct VSOutput {
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD;
	nointerpolation float4 TileRect	: COLOR;
	nointerpolation float3 info : TEXCOORD1;
	float3 worldPositon : TEXCOORD2;
};

float linearize_depth(float d, float zNear, float zFar)
{
	return zNear * zFar / (zFar + d * (zNear - zFar));
}

VSOutput VSMain(float3 Position : POSITION, float2 TexCoord : TEXCOORD0, float4 TileRect : COLOR, float3 info : TEXCOORD1)
{
	VSOutput result;
	float4 ModelPosition;
	ModelPosition = mul(modelMatrix, float4(Position, 1.0));
	result.Position = mul(mvp, float4(ModelPosition.xyz, 1.0));
	result.TexCoord = TexCoord;
	result.TileRect = TileRect;
	result.info = info;
	result.worldPositon.x = linearize_depth(result.Position.z / result.Position.w, 0.001, 3000);
	return result;
}

float4 unpack(float i)
{
	return fmod(float4(i / 262144.0, i / 4096.0, i / 64.0, i), 64.0);
}

// 
// Pixel Shader
//

Texture2D uTex0 : register(t1);
Texture2D uTex1 : register(t3);
Texture2D uTex2 : register(t4);

float4 PSMain(VSOutput input) : SV_TARGET
{
	float2 scaledUV = input.TexCoord;
	float tileWidth = input.TileRect.z;
	float tileHeight = input.TileRect.w;
	float shadeOffset = input.info.x;
	float visibility = input.info.y;
	float4 palint = unpack(input.info.z);

	int palette = int(palint[0]);
	int curbasepal = int(palint[1]);
	int paletteExtraBytes = int(palint[2]);

	palette = palette + paletteExtraBytes;

	float worldPositonLength = input.worldPositon.x;
	float shadeLookup = (worldPositonLength / 1.07 * visibility);
	shadeLookup = min(max(shadeLookup + shadeOffset, 0), 30);


	float colorIndex = 256;
	if (tileWidth > 0 && tileHeight > 0)
	{
		scaledUV.x = (frac(input.TexCoord.x) * tileWidth) + input.TileRect.x;
		scaledUV.y = (frac(input.TexCoord.y) * tileHeight) + input.TileRect.y;

		colorIndex = uTex0.Load(int3(scaledUV.x, scaledUV.y, 0)).r * 256;
	}
	else
	{
		uint width, height;
		uTex0.GetDimensions(width, height);
		colorIndex = uTex0.Load(int3(scaledUV.x * width, scaledUV.y * height, 0)).r * 256;
	}

	if (colorIndex == 256)
		discard;

	float colorIndexNear = uTex2.Load(int3(colorIndex, shadeLookup + (32 * palette), 0)).r * 256;
	float colorIndexFar = uTex2.Load(int3(colorIndex, shadeLookup + (32 * palette) + 1, 0)).r * 256;
	float3 texelNear = uTex1.Load(int3(colorIndexNear, curbasepal, 0)).xyz;
	float3 texelFar = uTex1.Load(int3(colorIndexFar, curbasepal, 0)).xyz;

	float3 tileArtColored = lerp(texelNear, texelFar, frac(shadeLookup));
#ifdef TRANSPARENT
	return float4(tileArtColored.x, tileArtColored.y, tileArtColored.z, 0.5);
#else
	return float4(tileArtColored.x, tileArtColored.y, tileArtColored.z, 1);
#endif
}