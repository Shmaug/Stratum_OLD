#include <Scene/Gizmos.hpp>
#include <Scene/Scene.hpp>
#include <Input/MouseKeyboardInput.hpp>

using namespace std;

const uint32_t CircleResolution = 64;
struct GizmoVertex {
	float3 position;
	float2 texcoord;
};
const ::VertexInput GizmoVertexInput {
	{
		0, // binding
		sizeof(GizmoVertex), // stride
		VK_VERTEX_INPUT_RATE_VERTEX // inputRate
	},
	{
		{
			0, // location
			0, // binding
			VK_FORMAT_R32G32B32_SFLOAT, // format
			0 // offset
		},
		{
			3, // location
			0, // binding
			VK_FORMAT_R32G32_SFLOAT, // format
			sizeof(float3) // offset
		}
	}
};

Gizmos::Gizmos(Scene* scene) : mLineVertexCount(0), mTriVertexCount(0) {
	mGizmoShader = scene->AssetManager()->LoadShader("Shaders/gizmo.shader");
	mWhiteTexture = scene->AssetManager()->LoadTexture("Assets/white.png");
	mTextures.push_back(mWhiteTexture);
	mTextureMap.emplace(mWhiteTexture, 0);
	
	GizmoVertex vertices[8 + CircleResolution];
	vertices[0] = { float3(-1,  1,  1), float2(0,0) };
	vertices[1] = { float3( 1,  1,  1), float2(1,0) };
	vertices[2] = { float3(-1, -1,  1), float2(0,1) };
	vertices[3] = { float3( 1, -1,  1), float2(1,1) };
	vertices[4] = { float3(-1,  1, -1), float2(0,0) };
	vertices[5] = { float3( 1,  1, -1), float2(1,0) };
	vertices[6] = { float3(-1, -1, -1), float2(0,1) };
	vertices[7] = { float3( 1, -1, -1), float2(1,1) };

	uint16_t indices[60 + CircleResolution*2] {
		// cube (x36)
		0,1,2,1,3,2, // +z
		7,5,4,7,4,6, // -z
		3,1,7,7,1,5, // +x
		4,0,2,6,4,2, // -x
		4,5,1,4,1,0, // +y
		7,2,3,7,6,2, // -y

		// wire cube (x24)
		0,1, 1,3, 3,2, 2,0,
		4,5, 5,7, 7,6, 6,4,
		0,4, 1,5, 3,7, 2,6
	};

	for (uint32_t i = 0; i < CircleResolution; i++) {
		vertices[8 + i].position.x = cosf(2.f * PI * i / (float)CircleResolution - PI * .5f);
		vertices[8 + i].position.y = sinf(2.f * PI * i / (float)CircleResolution - PI * .5f);
		vertices[8 + i].position.z = 0;
		vertices[8 + i].texcoord = vertices[8+i].position.xy;
		indices[60 + 2*i]   = 8 + i;
		indices[60 + 2*i+1] = 8 + (i+1) % CircleResolution;
	}

	for (uint32_t i = 0; i < scene->Instance()->DeviceCount(); i++){
		Device* device = scene->Instance()->GetDevice(i);
		DeviceData d = {};

		d.mVertices = new Buffer("Gizmo Vertices", device, vertices, sizeof(GizmoVertex) * (8 + CircleResolution), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		d.mIndices = new Buffer("Gizmo Indices", device, indices, sizeof(uint16_t) * (60 + 2*CircleResolution), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		d.mBufferIndex = new uint32_t[device->MaxFramesInFlight()];
		d.mInstanceBuffers = new vector<pair<DescriptorSet*, Buffer*>>[device->MaxFramesInFlight()];

		memset(d.mBufferIndex, 0, sizeof(uint32_t) * device->MaxFramesInFlight());

		mDeviceData.emplace(device, d);
	}
}
Gizmos::~Gizmos() {
	for (auto& d : mDeviceData){
		safe_delete(d.second.mVertices);
		safe_delete(d.second.mIndices);
		for (uint32_t i = 0; i < d.first->MaxFramesInFlight(); i++)
			for (auto& p : d.second.mInstanceBuffers[i]){
				safe_delete(p.first);
				safe_delete(p.second);
			}
		safe_delete_array(d.second.mBufferIndex);
		safe_delete_array(d.second.mInstanceBuffers);
	}
}

bool Gizmos::PositionHandle(const InputPointer* input, const quaternion& plane, float3& position, float radius, const float4& color){
	float t = input->mWorldRay.Intersect(Sphere(position, radius)).x;
	float lt = input->mLastWorldRay.Intersect(Sphere(position, radius)).x;

	DrawWireCircle(position, radius, plane, color);
	
	if (input->mAxis.at(0) < .5f || t < 0 || lt < 0) return false;

	float3 fwd = plane * float3(0,0,1);
	float3 p  = input->mWorldRay.mOrigin + input->mWorldRay.mDirection * input->mWorldRay.Intersect(fwd, position);
	float3 lp = input->mLastWorldRay.mOrigin + input->mLastWorldRay.mDirection * input->mLastWorldRay.Intersect(fwd, position);

	position += p - lp;

	return true;
}
bool Gizmos::RotationHandle(const InputPointer* input, const float3& center, quaternion& rotation, float radius){
	float t = input->mWorldRay.Intersect(Sphere(center, radius)).x;
	float lt = input->mLastWorldRay.Intersect(Sphere(center, radius)).x;

	quaternion r = rotation;
	DrawWireCircle(center, radius, r, float4(.2f,.2f,1,.5f));
	r *= quaternion(float3(0, PI/2, 0));
	DrawWireCircle(center, radius, r, float4(1,.2f,.2f,.5f));
	r *= quaternion(float3(PI/2, 0, 0));
	DrawWireCircle(center, radius, r, float4(.2f,1,.2f,.5f));

	if (input->mAxis.at(0) < .5f || t < 0 || lt < 0) return false;

	float3 p = input->mWorldRay.mOrigin - center + input->mWorldRay.mDirection * t;
	float3 lp = input->mLastWorldRay.mOrigin - center + input->mLastWorldRay.mDirection * lt;

	float3 rotAxis = cross(normalize(lp), normalize(p));
	float angle = length(rotAxis);
	if (fabsf(angle) > .0001f)
		rotation = quaternion(asinf(angle), rotAxis / angle) * rotation;

	return true;
}

void Gizmos::DrawLine(const float3& p0, const float3& p1, const float4& color){
	float3 v1 = float3(0,0,1);
	float3 v2 = p1 - p0;
	float l = length(v2);
	v2 /= l;

	DrawWireCube((p0 + p1) * .5f, float3(0, 0, l * .5f), quaternion(v1, v2), color);
}
void Gizmos::DrawBillboard(const float3& center, const float2& extents, const quaternion& rotation, const float4& color, Texture* texture, const float4& textureST){
	Gizmo g = {};
	g.Color = color;
	g.Position = center;
	g.Rotation = rotation;
	g.Scale = float3(extents, 0);
	if (mTextureMap.count(texture))
		g.TextureIndex = mTextureMap.at(texture);
	else {
		g.TextureIndex = (uint32_t)mTextures.size();
		mTextureMap.emplace(texture, (uint32_t)mTextures.size());
		mTextures.push_back(texture);
	}
	g.TextureST = textureST;
	g.Type = Billboard;
	mTriDrawList.push_back(g);
}
void Gizmos::DrawCube(const float3& center, const float3& extents, const quaternion& rotation, const float4& color) {
	Gizmo g = {};
	g.Color = color;
	g.Position = center;
	g.Rotation = rotation;
	g.Scale = extents;
	g.TextureIndex = 0;
	g.TextureST = 0;
	g.Type = Cube;
	mTriDrawList.push_back(g);
}
void Gizmos::DrawWireCube(const float3& center, const float3& extents, const quaternion& rotation, const float4& color) {
	Gizmo g = {};
	g.Color = color;
	g.Position = center;
	g.Rotation = rotation;
	g.Scale = extents;
	g.TextureIndex = 0;
	g.TextureST = 0;
	g.Type = Cube;
	mLineDrawList.push_back(g);
}
void Gizmos::DrawWireCircle(const float3& center, float radius, const quaternion& rotation, const float4& color) {
	Gizmo g = {};
	g.Color = color;
	g.Position = center;
	g.Rotation = rotation;
	g.Scale = radius;
	g.TextureIndex = 0;
	g.TextureST = 0;
	g.Type = Circle;
	mLineDrawList.push_back(g);
}
void Gizmos::DrawWireSphere(const float3& center, float radius, const float4& color) {
	DrawWireCircle(center, radius, quaternion(0,0,0,1), color);
	DrawWireCircle(center, radius, quaternion(0, .70710678f, 0, .70710678f), color);
	DrawWireCircle(center, radius, quaternion(.70710678f, 0, 0, .70710678f), color);
}

void Gizmos::PreFrame(Device* device) {
	DeviceData& data = mDeviceData.at(device);
	data.mBufferIndex[device->FrameContextIndex()] = 0;
}
void Gizmos::Draw(CommandBuffer* commandBuffer, Camera* camera) {
	uint32_t instanceOffset = 0;
	GraphicsShader* shader = mGizmoShader->GetGraphics(commandBuffer->Device(), {});
	DeviceData& data = mDeviceData.at(commandBuffer->Device());
	uint32_t frameContextIndex = commandBuffer->Device()->FrameContextIndex();

	uint32_t billboardCount = 0;
	uint32_t cubeCount = 0;
	uint32_t wireCubeCount = 0;
	uint32_t wireCircleCount = 0;

	for (const Gizmo& g : mLineDrawList) {
		if (g.Type == Cube) wireCubeCount++;
		else if (g.Type == Circle) wireCircleCount++;
	}
	for (const Gizmo& g : mTriDrawList) {
		if (g.Type == Cube) cubeCount++;
		else if (g.Type == Billboard) billboardCount++;
	}

	uint32_t total = billboardCount + cubeCount + wireCubeCount + wireCircleCount;
	if (total == 0) return;

	Buffer* gizmoBuffer = nullptr;
	DescriptorSet* gizmoDS = nullptr;
	if (data.mInstanceBuffers[frameContextIndex].size() <= data.mBufferIndex[frameContextIndex]) {
		gizmoBuffer = new Buffer("Gizmos", commandBuffer->Device(), sizeof(Gizmo) * total, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		gizmoBuffer->Map();

		gizmoDS = new DescriptorSet("Gizmos", commandBuffer->Device(), shader->mDescriptorSetLayouts[PER_OBJECT]);
		gizmoDS->CreateStorageBufferDescriptor(gizmoBuffer, 0, gizmoBuffer->Size(), OBJECT_BUFFER_BINDING);

		data.mInstanceBuffers[frameContextIndex].push_back(make_pair(gizmoDS, gizmoBuffer));
	} else {
		Buffer*& b = data.mInstanceBuffers[frameContextIndex][data.mBufferIndex[frameContextIndex]].second;
		gizmoDS = data.mInstanceBuffers[frameContextIndex][data.mBufferIndex[frameContextIndex]].first;

		if (b->Size() < sizeof(Gizmo) * total) {
			safe_delete(b);
			b = new Buffer("Gizmos", commandBuffer->Device(), sizeof(Gizmo) * total, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			b->Map();
			gizmoDS->CreateStorageBufferDescriptor(b, 0, b->Size(), OBJECT_BUFFER_BINDING);
		}

		gizmoBuffer = b;
	}

	VkDescriptorSetLayoutBinding b = shader->mDescriptorBindings.at("MainTexture").second;
	gizmoDS->CreateSampledTextureDescriptor(mTextures.data(), (uint32_t)mTextures.size(), b.descriptorCount, b.binding);

	data.mBufferIndex[frameContextIndex]++;

	sort(mLineDrawList.begin(), mLineDrawList.end(), [&](const Gizmo& a, const Gizmo& b){
		return a.Type < b.Type;
	});
	sort(mTriDrawList.begin(), mTriDrawList.end(), [&](const Gizmo& a, const Gizmo& b){
		return a.Type < b.Type;
	});

	Gizmo* buf = (Gizmo*)gizmoBuffer->MappedData();
	if (mLineDrawList.size()){
		memcpy(buf, mLineDrawList.data(), mLineDrawList.size() * sizeof(Gizmo));
		buf += mLineDrawList.size();
	}
	if (mTriDrawList.size())
		memcpy(buf, mTriDrawList.data(), mTriDrawList.size() * sizeof(Gizmo));

	if (wireCubeCount + wireCircleCount > 0) {
		VkPipelineLayout layout = commandBuffer->BindShader(shader, &GizmoVertexInput, camera, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
		if (layout) {
			VkDeviceSize vboffset = 0;
			VkBuffer vb = *data.mVertices;
			vkCmdBindVertexBuffers(*commandBuffer, 0, 1, &vb, &vboffset);
			vkCmdBindIndexBuffer(*commandBuffer, *data.mIndices, 0, VK_INDEX_TYPE_UINT16);

			VkDescriptorSet ds = *gizmoDS;
			vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &ds, 0, nullptr);

			// wire cube	
			vkCmdDrawIndexed(*commandBuffer, 24, wireCubeCount, 36, 0, instanceOffset);
			instanceOffset += wireCubeCount;
			// wire circle
			vkCmdDrawIndexed(*commandBuffer, CircleResolution * 2, wireCircleCount, 60, 0, instanceOffset);
			instanceOffset += wireCircleCount;
		}
	}

	if (cubeCount + billboardCount > 0){
		VkPipelineLayout layout = commandBuffer->BindShader(shader, &GizmoVertexInput, camera, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		if (layout) {
			VkDeviceSize vboffset = 0;
			VkBuffer vb = *data.mVertices;
			vkCmdBindVertexBuffers(*commandBuffer, 0, 1, &vb, &vboffset);
			vkCmdBindIndexBuffer(*commandBuffer, *data.mIndices, 0, VK_INDEX_TYPE_UINT16);

			VkDescriptorSet ds = *gizmoDS;
			vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &ds, 0, nullptr);

			// billboard
			vkCmdDrawIndexed(*commandBuffer, 6, billboardCount, 0, 0, instanceOffset);
			instanceOffset += billboardCount;
			// cube	
			vkCmdDrawIndexed(*commandBuffer, 36, cubeCount, 0, 0, instanceOffset);
			instanceOffset += cubeCount;
		}
	}

	mTriDrawList.clear();
	mLineDrawList.clear();
	mTextures.clear();
	mTextureMap.clear();
	mTextures.push_back(mWhiteTexture);
	mTextureMap.emplace(mWhiteTexture, 0);
}