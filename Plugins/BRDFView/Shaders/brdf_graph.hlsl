#pragma vertex vsmain
#pragma fragment fsmain main

#pragma render_queue 1000

#include <include/shadercompat.h>
#include <include/disney.hlsli>

// per-object
[[vk::binding(INSTANCE_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<InstanceBuffer> Instances : register(t0);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	STRATUM_PUSH_CONSTANTS

	float Metallic;
	float Specular;
	float Anisotropy;
	float Roughness;
	float SpecularTint;
	float SheenTint;
	float Sheen;
	float ClearcoatGloss;
	float Clearcoat;
	float Subsurface;
	float Transmission;

	float3 LocalLightDirection;
};

#include <include/util.hlsli>

struct v2f {
	float4 position : SV_Position;
	float4 worldPos : TEXCOORD0;
	float3 wo : TEXCOORD1;
};

v2f vsmain(
	[[vk::location(0)]] float3 vertex : POSITION,
	[[vk::location(1)]] float3 normal : NORMAL,
	uint instance : SV_InstanceID) {
	v2f o;

	float4x4 ct = float4x4(
		1, 0, 0, -Camera.Position.x,
		0, 1, 0, -Camera.Position.y,
		0, 0, 1, -Camera.Position.z,
		0, 0, 0, 1);
	float4 worldPos = mul(mul(ct, Instances[instance].ObjectToWorld), float4(vertex, 1.0));

	o.position = mul(STRATUM_MATRIX_VP, worldPos);
	StratumOffsetClipPosStereo(o.position);
	o.worldPos = float4(worldPos.xyz, o.position.z);

	o.wo = normal;

	return o;
}

void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {
	depthNormal = float4(normalize(cross(ddx(i.worldPos.xyz), ddy(i.worldPos.xyz))) * i.worldPos.w, 1);

	float3 wo = normalize(i.wo);

	DisneyMaterial mat = {};
	mat.BaseColor = 1;
	mat.Metallic = Metallic;
	mat.Specular = Specular;
	mat.Anisotropy = Anisotropy;
	mat.Roughness = Roughness;
	mat.SpecularTint = SpecularTint;
	mat.SheenTint = SheenTint;
	mat.Sheen = Sheen;
	mat.ClearcoatGloss = ClearcoatGloss;
	mat.Clearcoat = Clearcoat;
	mat.Subsurface = Subsurface;
	mat.Transmission = Transmission;

	float pdf = Disney_GetPdf(mat, LocalLightDirection, wo);

	color = float4(lerp(float3(.25, .25, 1), float3(1, .25, .25), pdf), 1);
	color.rgb *= saturate(abs(wo.y) * 256);
}