#pragma vertex vsmain
#pragma fragment fsmain main
#pragma fragment fsdepth depth

#pragma multi_compile ENABLE_SCATTERING ENVIRONMENT_TEXTURE

#pragma multi_compile ALPHA_CLIP
#pragma multi_compile TEXTURED

#pragma cull false
#pragma render_queue 1000

#pragma static_sampler Sampler
#pragma static_sampler ShadowSampler maxAnisotropy=0 maxLod=0 addressMode=clamp_border borderColor=float_opaque_white compareOp=less
#pragma static_sampler AtmosphereSampler maxAnisotropy=0 addressMode=clamp_edge

#include "include/shadercompat.h"

struct ClothVertex {
	float4 Position;
	float4 Velocity;
};

// per-object
[[vk::binding(INSTANCE_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ClothVertex> Vertices : register(t0);
[[vk::binding(LIGHT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<GPULight> Lights : register(t1);
[[vk::binding(SHADOW_ATLAS_BINDING, PER_OBJECT)]] Texture2D<float> ShadowAtlas : register(t2);
[[vk::binding(SHADOW_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ShadowData> Shadows : register(t3);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 0, PER_MATERIAL)]] Texture2D<float4> MainTexture		: register(t4);
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] Texture2D<float4> NormalTexture	: register(t12);
[[vk::binding(BINDING_START + 2, PER_MATERIAL)]] Texture2D<float4> MaskTexture		: register(t20); // rgba ->ao, rough, metallic (glTF spec.)

[[vk::binding(BINDING_START + 3, PER_MATERIAL)]] Texture3D<float3> InscatteringLUT		: register(t29);
[[vk::binding(BINDING_START + 4, PER_MATERIAL)]] Texture3D<float3> ExtinctionLUT		: register(t30);
[[vk::binding(BINDING_START + 5, PER_MATERIAL)]] Texture2D<float>  LightShaftLUT		: register(t31);

[[vk::binding(BINDING_START + 6, PER_MATERIAL)]] Texture2D<float4> EnvironmentTexture	: register(t32);

[[vk::binding(BINDING_START + 7, PER_MATERIAL)]] SamplerState Sampler : register(s0);
[[vk::binding(BINDING_START + 8, PER_MATERIAL)]] SamplerComparisonState ShadowSampler : register(s1);
[[vk::binding(BINDING_START + 9, PER_MATERIAL)]] SamplerState AtmosphereSampler : register(s2);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	STRATUM_PUSH_CONSTANTS

	float4x4 ObjectToWorld;
	uint ClothResolution;

	float4 Color;
	float Metallic;
	float Roughness;
	float BumpStrength;
	float3 Emission;

	float4 TextureST;
};

//#define SHOW_CASCADE_SPLITS

#include "include/util.hlsli"
#include "include/shadow.hlsli"
#include "include/brdf.hlsli"
#include "include/scatter.hlsli"

struct v2f {
	float4 position : SV_Position;
	float4 worldPos : TEXCOORD0;
	float4 screenPos : TEXCOORD1;
	float3 normal : NORMAL;
	#ifdef TEXTURED
	float3 tangent : TANGENT;
	float2 texcoord : TEXCOORD2;
	#endif
};

const uint indicies[6] = {
	0,1,3,
	0,3,2
};
v2f vsmain(uint vIndex : SV_VertexID) {
	uint index = vIndex / 6;
	uint2 xy = uint2(index % (ClothResolution-1), index / (ClothResolution-1));

	float3 v[4] = {
		Vertices[xy.y * ClothResolution + xy.x].Position.xyz,
		Vertices[xy.y * ClothResolution + (xy.x + 1)].Position.xyz,
		Vertices[(xy.y + 1) * ClothResolution + xy.x].Position.xyz,
		Vertices[(xy.y + 1) * ClothResolution + (xy.x + 1)].Position.xyz
	};
	
	float3 vertex = v[indicies[vIndex % 6]];
	float3 normal = 0;
	float3 tangent = v[1] - v[0];
	float2 texcoord = float2(xy) / (ClothResolution - 1);

	normal += cross(v[1] - v[0], v[3] - v[0]);
	normal += cross(v[3] - v[0], v[2] - v[0]);
	float3 w0, w1;
	if (xy.x > 0) {
		w0 = Vertices[xy.y * ClothResolution + (xy.x - 1)].Position.xyz;
		normal += cross(w0 - v[0], v[1] - v[0]);
	}
	if (xy.y > 0) {
		w1 = Vertices[(xy.y - 1) * ClothResolution + xy.x].Position.xyz;
		normal += cross(w1 - v[0], v[1] - v[0]);
	}
	if (xy.x > 0 && xy.y > 0) {
		float3 w2 = Vertices[(xy.y - 1) * ClothResolution + (xy.x - 1)].Position.xyz;
		normal += cross(w2 - v[0], w0 - v[0]);
		normal += cross(w1 - v[0], w2 - v[0]);
	}
	normal = normalize(normal);

	v2f o;
	
	float4x4 ct = float4x4(
		1,0,0,-Camera.Position.x,
		0,1,0,-Camera.Position.y,
		0,0,1,-Camera.Position.z,
		0,0,0,1);
	float4 worldPos = mul(mul(ct, ObjectToWorld), float4(vertex, 1.0));

	o.position = mul(STRATUM_MATRIX_VP, worldPos);
	StratumOffsetClipPosStereo(o.position);
	o.worldPos = float4(worldPos.xyz, o.position.z);
	
	o.screenPos = ComputeScreenPos(o.position);
	o.normal = mul((float3x3)ObjectToWorld, normal);
	
	#ifdef TEXTURED
	o.tangent = mul((float3x3)ObjectToWorld, tangent);
	o.texcoord = texcoord * TextureST.xy + TextureST.zw;
	#endif

	return o;
}

#ifdef ALPHA_CLIP
float fsdepth(in float4 worldPos : TEXCOORD0, in float2 texcoord : TEXCOORD2) : SV_Target0 {
	clip((MainTexture.Sample(Sampler, texcoord) * Color).a - .75);
#else
float fsdepth(in float4 worldPos : TEXCOORD0) : SV_Target0 {
#endif
	return worldPos.w;
}

void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {
	depthNormal = float4(normalize(cross(ddx(i.worldPos.xyz), ddy(i.worldPos.xyz))) * i.worldPos.w, 1);

	float3 view = ComputeView(i.worldPos.xyz, i.screenPos);

	#ifdef TEXTURED
	float4 col = MainTexture.Sample(Sampler, i.texcoord) * Color;
	#else
	float4 col = Color;
	#endif

	float3 normal = normalize(i.normal);
	if (dot(normal, view) < 0) normal = -normal;

	#ifdef TEXTURED
	float4 bump = NormalTexture.Sample(Sampler, i.texcoord);
	bump.xyz = bump.xyz * 2 - 1;
	float3 tangent = normalize(i.tangent);
	float3 bitangent = normalize(cross(i.normal, i.tangent));
	bump.xy *= BumpStrength;
	normal = normalize(tangent * bump.x + bitangent * bump.y + normal * bump.z);
	#endif

	#ifdef TEXTURED
	float4 mask = MaskTexture.Sample(Sampler, i.texcoord);
	#else
	float4 mask = 1;
	#endif

	MaterialInfo material;
	material.diffuse = DiffuseAndSpecularFromMetallic(col.rgb, Metallic*mask.b, material.specular, material.oneMinusReflectivity);
	material.perceptualRoughness = Roughness * mask.g * .99;
	material.roughness = max(.002, material.perceptualRoughness * material.perceptualRoughness);
	material.occlusion = mask.r;
	material.emission = Emission;

	float3 eval = EvaluateLighting(material, i.worldPos.xyz, normal, view, i.worldPos.w);

	#ifdef ENABLE_SCATTERING
	ApplyScattering(eval, i.screenPos.xy / i.screenPos.w, i.worldPos.w);
	#endif

	color = float4(eval, col.a);
	depthNormal.a = col.a;
}