#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 5000
#pragma cull false
#pragma blend alpha

#pragma static_sampler Sampler

#pragma multi_compile SCREEN_SPACE

#include <include/shadercompat.h>

struct Glyph {
	float2 position;
	float2 size;
	float2 uv;
	float2 uvsize;
};

// per-object
[[vk::binding(BINDING_START + 0, PER_OBJECT)]] Texture2D<float4> MainTexture : register(t0);
[[vk::binding(BINDING_START + 1, PER_OBJECT)]] StructuredBuffer<float4x4> Transforms : register(t1);
[[vk::binding(BINDING_START + 2, PER_OBJECT)]] StructuredBuffer<Glyph> Glyphs : register(t2);
[[vk::binding(BINDING_START + 3, PER_OBJECT)]] SamplerState Sampler : register(s0);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4 StereoClipTransform;
	float4 Color;
	float4 Bounds;
	float2 ScreenSize;
	float2 Offset;
	float Depth;
	uint StereoEye;
}

#include <include/util.hlsli>

struct v2f {
	float4 position : SV_Position;
	float4 texcoord : TEXCOORD0;
#ifndef SCREEN_SPACE
	float4 worldPos : TEXCOORD1;
#endif
};

v2f vsmain(uint id : SV_VertexId, uint instance : SV_InstanceID) {
	uint g = id / 6;
	uint c = id % 6;
	
	static const float2 offsets[6] = {
		float2(0,0),
		float2(1,0),
		float2(0,1),
		float2(1,0),
		float2(1,1),
		float2(0,1)
	};

	float2 p = Glyphs[g].position + Glyphs[g].size * offsets[c] + Offset;

	v2f o;
#ifdef SCREEN_SPACE
	o.position = float4((p / ScreenSize) * 2 - 1, Depth, 1);
	o.position.y = -o.position.y;
#else
	float4x4 ct = float4x4(1,0,0,-Camera.Position.x, 0,1,0,-Camera.Position.y, 0,0,1,-Camera.Position.z, 0,0,0,1);
	float4 worldPos = mul(mul(ct, Transforms[instance]), float4(p, 0, 1));
	o.position = mul(STRATUM_MATRIX_VP, worldPos);
	StratumOffsetClipPosStereo(o.position);
	o.worldPos = float4(worldPos.xyz, o.position.z);
#endif
	o.texcoord.xy = Glyphs[g].uv + Glyphs[g].uvsize * offsets[c];
	o.texcoord.zw = (p - Bounds.xy) / Bounds.zw;
	return o;
}

float4 SampleFont(float2 uv){
	float2 dx = ddx(uv.xy);
	float2 dy = ddy(uv.xy);
	float4 oxy = float4(dx, dy) * float4(0.125, 0.375, 0.125, 0.375);
	float4 oyx = float4(dy, dx) * float4(0.125, 0.375, 0.125, 0.375);
	float4 col = 0;
	col += MainTexture.SampleBias(Sampler, uv + oxy.xy, -1);
	col += MainTexture.SampleBias(Sampler, uv - oxy.xy - oxy.zw, -1);
	col += MainTexture.SampleBias(Sampler, uv + oyx.zw - oyx.xy, -1);
	col += MainTexture.SampleBias(Sampler, uv - oyx.zw + oyx.xy, -1);
	return col * 0.25;
}

void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1 ) {
	#ifdef SCREEN_SPACE
	depthNormal = 0;
	color = MainTexture.SampleLevel(Sampler, i.texcoord.xy, 0) * Color;
	#else
	depthNormal = float4(normalize(cross(ddx(i.worldPos.xyz), ddy(i.worldPos.xyz))) * i.worldPos.w, 1);
	color = SampleFont(i.texcoord.xy) * Color;
	#endif
	color.a *= i.texcoord.z > 0 && i.texcoord.w > 0 && i.texcoord.z < 1 && i.texcoord.w < 1;
	depthNormal.a = color.a;
}