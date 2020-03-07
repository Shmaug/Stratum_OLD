#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 5000
#pragma cull false
#pragma zwrite false
#pragma ztest false

#pragma static_sampler Sampler

[[vk::binding(0, 0)]] Texture2D<float4> Radiance : register(t0);
[[vk::binding(3, 0)]] SamplerState Sampler : register(s0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4 ScaleTranslate;
	float4 TextureST;
	float Exposure;
}

struct v2f {
	float4 position : SV_Position;
	float2 texcoord : TEXCOORD1;
};

v2f vsmain(uint index : SV_VertexID) {
	static const float2 positions[6] = {
		float2(0,0),
		float2(1,0),
		float2(1,1),
		float2(0,1),
		float2(0,0),
		float2(1,1)
	};

	float2 p = positions[index] * ScaleTranslate.xy + ScaleTranslate.zw;
	
	v2f o;
	o.position = float4(p * 2 - 1, .01, 1);
	o.position.y = -o.position.y;
	o.texcoord = positions[index] * TextureST.xy + TextureST.zw;

	return o;
}

static const float3x3 ACESInputMat = float3x3(
	float3(0.59719, 0.35458, 0.04823),
	float3(0.07600, 0.90834, 0.01566),
	float3(0.02840, 0.13383, 0.83777) );
static const float3x3 ACESOutputMat = float3x3(
	float3( 1.60475, -0.53108, -0.07367),
	float3(-0.10208,  1.10813, -0.00605),
	float3(-0.00327, -0.07276,  1.07602) );

float3 RRTAndODTFit(float3 v) {
	float3 a = v * (v + 0.0245786f) - 0.000090537f;
	float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
	return a / b;
}

void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {
	depthNormal = 0;

	float4 s = Radiance.SampleLevel(Sampler, i.texcoord, 0);

	float3 radiance = Exposure * (s.rgb / s.w);
	radiance = mul(ACESInputMat, radiance);
	radiance = RRTAndODTFit(radiance);
	radiance = mul(ACESOutputMat, radiance);
	color = float4(saturate(radiance), 1);
}