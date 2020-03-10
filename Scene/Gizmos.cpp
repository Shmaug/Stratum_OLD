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
		{
			0, // binding
			sizeof(GizmoVertex), // stride
			VK_VERTEX_INPUT_RATE_VERTEX // inputRate
		}
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

Buffer* Gizmos::mVertices;
Buffer* Gizmos::mIndices;

uint32_t* Gizmos::mBufferIndex;
std::vector<std::pair<DescriptorSet*, Buffer*>>* Gizmos::mInstanceBuffers;

Texture* Gizmos::mWhiteTexture;

std::vector<Texture*> Gizmos::mTextures;
std::unordered_map<Texture*, uint32_t> Gizmos::mTextureMap;

uint32_t Gizmos::mLineVertexCount;
uint32_t Gizmos::mTriVertexCount;

std::vector<Gizmos::Gizmo> Gizmos::mTriDrawList;
std::vector<Gizmos::Gizmo> Gizmos::mLineDrawList;

size_t Gizmos::mHotControl;

void Gizmos::Initialize(Device* device, AssetManager* assetManager) {
	mLineVertexCount = 0;
	mTriVertexCount = 0;
	mHotControl = -1;

	mWhiteTexture = assetManager->LoadTexture("Assets/Textures/white.png");
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

	mVertices = new Buffer("Gizmo Vertices", device, vertices, sizeof(GizmoVertex) * (8 + CircleResolution), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	mIndices = new Buffer("Gizmo Indices", device, indices, sizeof(uint16_t) * (60 + 2*CircleResolution), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	mBufferIndex = new uint32_t[device->MaxFramesInFlight()];
	mInstanceBuffers = new vector<pair<DescriptorSet*, Buffer*>>[device->MaxFramesInFlight()];

	memset(mBufferIndex, 0, sizeof(uint32_t) * device->MaxFramesInFlight());
}
void Gizmos::Destroy(Device* device) {
	safe_delete(mVertices);
	safe_delete(mIndices);
	for (uint32_t i = 0; i < device->MaxFramesInFlight(); i++)
		for (auto& p : mInstanceBuffers[i]){
			safe_delete(p.first);
			safe_delete(p.second);
		}
	safe_delete_array(mBufferIndex);
	safe_delete_array(mInstanceBuffers);
}

bool Gizmos::PositionHandle(const string& name, const InputPointer* input, const quaternion& plane, float3& position, float radius, const float4& color) {
	size_t controlId = hash<string>()(name);

	float2 t;
	bool th = input->mWorldRay.Intersect(Sphere(position, radius), t);
	float2 lt;
	bool lth = input->mLastWorldRay.Intersect(Sphere(position, radius), lt);

	DrawWireCircle(position, radius, plane, color);
	
	if (input->mAxis.at(0) < .5f) {
		if (mHotControl == controlId) mHotControl = -1;
		return false;
	}
	if (mHotControl != controlId && (!th || !lth || t.x < 0 || lt.x < 0)) return false;
	mHotControl = controlId;

	float3 fwd = plane * float3(0,0,1);
	float3 p  = input->mWorldRay.mOrigin + input->mWorldRay.mDirection * input->mWorldRay.Intersect(fwd, position);
	float3 lp = input->mLastWorldRay.mOrigin + input->mLastWorldRay.mDirection * input->mLastWorldRay.Intersect(fwd, position);

	position += p - lp;

	return true;
}
bool Gizmos::RotationHandle(const string& name, const InputPointer* input, const float3& center, quaternion& rotation, float radius, float sensitivity) {
	size_t controlId = hash<string>()(name);

	float2 t;
	float2 lt;
	bool th = input->mWorldRay.Intersect(Sphere(center, radius), t);
	bool lth = input->mLastWorldRay.Intersect(Sphere(center, radius), lt);

	quaternion r = rotation;
	DrawWireCircle(center, radius, r, float4(.2f,.2f,1,.5f));
	r *= quaternion(float3(0, PI/2, 0));
	DrawWireCircle(center, radius, r, float4(1,.2f,.2f,.5f));
	r *= quaternion(float3(PI/2, 0, 0));
	DrawWireCircle(center, radius, r, float4(.2f,1,.2f,.5f));

	if (input->mAxis.at(0) < .5f) {
		if (mHotControl == controlId) mHotControl = -1;
		return false;
	}
	if (mHotControl != controlId && (!th || !lth || t.x < 0 || lt.x < 0)) return false;
	mHotControl = controlId;

	if (!th) t.x = input->mWorldRay.Intersect(normalize(input->mWorldRay.mOrigin - center), center);
	if (!lth) lt.x = input->mLastWorldRay.Intersect(normalize(input->mLastWorldRay.mOrigin - center), center);

	float3 p = input->mWorldRay.mOrigin - center + input->mWorldRay.mDirection * t.x;
	float3 lp = input->mLastWorldRay.mOrigin - center + input->mLastWorldRay.mDirection * lt.x;

	float3 rotAxis = cross(normalize(lp), normalize(p));
	float angle = length(rotAxis);
	if (fabsf(angle) > .0001f)
		rotation = quaternion(asinf(angle) * sensitivity, rotAxis / angle) * rotation;

	return true;
}

void Gizmos::DrawLine(const float3& p0, const float3& p1, const float4& color){
	float3 v2 = p1 - p0;
	float l = length(v2);
	v2 /= l;

	DrawWireCube((p0 + p1) * .5f, float3(0, 0, l * .5f), quaternion::FromTo(float3(0,0,1), v2), color);
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

void Gizmos::PreFrame(Scene* scene) {
	mBufferIndex[scene->Instance()->Device()->FrameContextIndex()] = 0;
	mTriDrawList.clear();
	mLineDrawList.clear();
	mTextures.clear();
	mTextureMap.clear();
	mTextures.push_back(mWhiteTexture);
	mTextureMap.emplace(mWhiteTexture, 0);
}
void Gizmos::Draw(CommandBuffer* commandBuffer, PassType pass, Camera* camera) {
	uint32_t instanceOffset = 0;
	GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/gizmo.stm")->GetGraphics(pass, {});
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
	if (mInstanceBuffers[frameContextIndex].size() <= mBufferIndex[frameContextIndex]) {
		gizmoBuffer = new Buffer("Gizmos", commandBuffer->Device(), sizeof(Gizmo) * total, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

		gizmoDS = new DescriptorSet("Gizmos", commandBuffer->Device(), shader->mDescriptorSetLayouts[PER_OBJECT]);
		gizmoDS->CreateStorageBufferDescriptor(gizmoBuffer, 0, gizmoBuffer->Size(), INSTANCE_BUFFER_BINDING);

		mInstanceBuffers[frameContextIndex].push_back(make_pair(gizmoDS, gizmoBuffer));
	} else {
		Buffer*& b = mInstanceBuffers[frameContextIndex][mBufferIndex[frameContextIndex]].second;
		gizmoDS = mInstanceBuffers[frameContextIndex][mBufferIndex[frameContextIndex]].first;

		if (b->Size() < sizeof(Gizmo) * total) {
			safe_delete(b);
			b = new Buffer("Gizmos", commandBuffer->Device(), sizeof(Gizmo) * total, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
			gizmoDS->CreateStorageBufferDescriptor(b, 0, b->Size(), INSTANCE_BUFFER_BINDING);
		}

		gizmoBuffer = b;
	}

	VkDescriptorSetLayoutBinding b = shader->mDescriptorBindings.at("MainTexture").second;
	for (uint32_t i = 0; i < mTextures.size(); i++)
		gizmoDS->CreateSampledTextureDescriptor(mTextures[i], i, b.binding);
	for (uint32_t i = mTextures.size(); i < b.descriptorCount; i++)
		gizmoDS->CreateSampledTextureDescriptor(mTextures[0], i, b.binding);
	gizmoDS->FlushWrites();

	mBufferIndex[frameContextIndex]++;

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
		VkPipelineLayout layout = commandBuffer->BindShader(shader, pass, &GizmoVertexInput, camera, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
		if (layout) {
			commandBuffer->BindVertexBuffer(mVertices, 0, 0);
			commandBuffer->BindIndexBuffer(mIndices, 0, VK_INDEX_TYPE_UINT16);

			vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, *gizmoDS, 0, nullptr);

			// wire cube
			camera->SetStereo(commandBuffer, shader, EYE_LEFT);
			vkCmdDrawIndexed(*commandBuffer, 24, wireCubeCount, 36, 0, instanceOffset);
			if (camera->StereoMode() != STEREO_NONE) {
				camera->SetStereo(commandBuffer, shader, EYE_RIGHT);
				vkCmdDrawIndexed(*commandBuffer, 24, wireCubeCount, 36, 0, instanceOffset);
			}

			instanceOffset += wireCubeCount;

			// wire circle
			camera->SetStereo(commandBuffer, shader, EYE_LEFT);
			vkCmdDrawIndexed(*commandBuffer, CircleResolution * 2, wireCircleCount, 60, 0, instanceOffset);
			if (camera->StereoMode() != STEREO_NONE) {
				camera->SetStereo(commandBuffer, shader, EYE_RIGHT);
				vkCmdDrawIndexed(*commandBuffer, CircleResolution * 2, wireCircleCount, 60, 0, instanceOffset);
			}

			instanceOffset += wireCircleCount;
		}
	}

	if (cubeCount + billboardCount > 0){
		VkPipelineLayout layout = commandBuffer->BindShader(shader, pass, &GizmoVertexInput, camera, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		if (layout) {
			commandBuffer->BindVertexBuffer(mVertices, 0, 0);
			commandBuffer->BindIndexBuffer(mIndices, 0, VK_INDEX_TYPE_UINT16);

			vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, *gizmoDS, 0, nullptr);

			// billboard
			camera->SetStereo(commandBuffer, shader, EYE_LEFT);
			vkCmdDrawIndexed(*commandBuffer, 6, billboardCount, 0, 0, instanceOffset);
			if (camera->StereoMode() != STEREO_NONE) {
				camera->SetStereo(commandBuffer, shader, EYE_RIGHT);
				vkCmdDrawIndexed(*commandBuffer, 6, billboardCount, 0, 0, instanceOffset);
			}

			instanceOffset += billboardCount;

			// cube	
			camera->SetStereo(commandBuffer, shader, EYE_LEFT);
			vkCmdDrawIndexed(*commandBuffer, 36, cubeCount, 0, 0, instanceOffset);
			if (camera->StereoMode() != STEREO_NONE) {
				camera->SetStereo(commandBuffer, shader, EYE_RIGHT);
				vkCmdDrawIndexed(*commandBuffer, 36, cubeCount, 0, 0, instanceOffset);
			}

			instanceOffset += cubeCount;
		}
	}
}