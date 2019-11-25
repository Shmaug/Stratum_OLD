#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 5000
#pragma cull false
#pragma zwrite false
#pragma blend alpha

#pragma static_sampler Sampler
#pragma multi_compile CANVAS_BOUNDS

#include <shadercompat.h>

struct Glyph {
	float2 position;
	float2 size;
	float2 uv;
	float2 uvsize;
};

// per-object
[[vk::binding(BINDING_START + 0, PER_OBJECT)]] Texture2D<float4> MainTexture : register(t0);
[[vk::binding(BINDING_START + 1, PER_OBJECT)]] SamplerState Sampler : register(s0);
[[vk::binding(BINDING_START + 2, PER_OBJECT)]] StructuredBuffer<Glyph> Glyphs : register(t1);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4x4 WorldViewProjection;
	float3 WorldNormal;
	float4 Color;
	float2 Offset;
	float2 Bounds;
}

struct v2f {
	float4 position : SV_Position;
	float depth : TEXCOORD0;
	#ifdef CANVAS_BOUNDS
	float4 texcoord : TEXCOORD1;
	#else
	float2 texcoord : TEXCOORD1;
	#endif
};

v2f vsmain(uint id : SV_VertexId) {
	v2f o;

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

	#ifdef CANVAS_BOUNDS
	o.texcoord.zw = abs(p);
	#endif

	o.position = mul(WorldViewProjection, float4(p, 0, 1));
	o.depth = o.position.w / Camera.Viewport.w;
	o.texcoord.xy = Glyphs[g].uv + Glyphs[g].uvsize * offsets[c];

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
	#ifdef CANVAS_BOUNDS
	clip(any(Bounds - i.texcoord.zw));
	#endif
	color = SampleFont(i.texcoord.xy) * Color;
	depthNormal = float4(normalize(WorldNormal) * .5 + .5, i.depth);
}