#include <Core/EnginePlugin.hpp>

#include <Scene/Camera.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>

#include <assimp/pbrmaterial.h>

using namespace std;

#ifdef GetObject
#undef GetObject
#endif

#define PASS_RAYTRACE (1u << 23)

#pragma pack(push)
#pragma pack(1)
struct DisneyMaterial {
	float3 BaseColor;
	float Metallic;
	float3 Emission;
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
	uint32_t pad[3];
};

struct GpuBvhNode {
	float3 Min;
	uint32_t StartIndex;
	float3 Max;
	uint32_t PrimitiveCount;
	uint32_t RightOffset; // 1st child is at node[index + 1], 2nd child is at node[index + mRightOffset]
	uint32_t pad[3];
};
struct GpuLeafNode {
	float4x4 NodeToWorld;
	float4x4 WorldToNode;
	uint32_t RootIndex;
	uint32_t MaterialIndex;
	uint32_t pad[2];
};
#pragma pack(pop)

class Raytracing : public EnginePlugin {
private:
	vector<Object*> mObjects;
	Scene* mScene;
	uint32_t mFrameIndex;

	struct FrameData {
		float4x4 mViewProjection;
		float4x4 mInvViewProjection;
		float3 mCameraPosition;
		Texture* mPrimary;
		Texture* mMeta;
		Buffer* mNodes;
		Buffer* mLeafNodes;
		Buffer* mVertices;
		Buffer* mTriangles;
		Buffer* mLights;
		Buffer* mMaterials;
		uint32_t mBvhBase;
		uint32_t mLightCount;
		uint64_t mLastBuild;
		unordered_map<Mesh*, uint32_t> mMeshes; // Mesh, RootIndex
	};
	FrameData* mFrameData;
	
	void Build(CommandBuffer* commandBuffer, FrameData& fd) {
		PROFILER_BEGIN("Copy BVH");
		ObjectBvh2* sceneBvh = mScene->BVH();

		fd.mMeshes.clear();

		vector<GpuBvhNode> nodes;
		vector<GpuLeafNode> leafNodes;
		vector<DisneyMaterial> materials;
		vector<uint3> triangles;
		vector<uint4> lights;

		uint32_t nodeBaseIndex = 0;
		uint32_t vertexCount = 0;
		
		unordered_map<Mesh*, uint2> triangleRanges;
		unordered_map<Buffer*, vector<VkBufferCopy>> vertexCopies;

		PROFILER_BEGIN("Copy meshes");
		// Copy mesh BVHs
		for (uint32_t sni = 0; sni < sceneBvh->Nodes().size(); sni++){
			const ObjectBvh2::Node& sn = sceneBvh->Nodes()[sni];
			if (sn.mRightOffset == 0) {
				for (uint32_t i = 0; i < sn.mCount; i++) {
					MeshRenderer* mr = dynamic_cast<MeshRenderer*>(sceneBvh->GetObject(sn.mStartIndex + i));
					if (mr && mr->Visible()) {
						leafNodes.push_back({});
						Mesh* m = mr->Mesh();

						if (!fd.mMeshes.count(m)) {
							fd.mMeshes.emplace(m, nodeBaseIndex);

							TriangleBvh2* bvh = m->BVH();
							nodes.resize(nodeBaseIndex + bvh->Nodes().size());

							uint32_t baseTri = triangles.size();

							for (uint32_t ni = 0; ni < bvh->Nodes().size(); ni++) {
								const TriangleBvh2::Node& n = bvh->Nodes()[ni];
								GpuBvhNode& gn = nodes[nodeBaseIndex + ni];
								gn.RightOffset = n.mRightOffset;
								gn.Min = n.mBounds.mMin;
								gn.Max = n.mBounds.mMax;
								gn.StartIndex = triangles.size();
								gn.PrimitiveCount = n.mCount;

								if (n.mRightOffset == 0)
									for (uint32_t i = 0; i < n.mCount; i++)
										triangles.push_back(vertexCount + bvh->GetTriangle(n.mStartIndex + i));
							}

							triangleRanges.emplace(m, uint2(baseTri, triangles.size()));

							auto& cpy = vertexCopies[m->VertexBuffer().get()];
							VkBufferCopy rgn = {};
							rgn.srcOffset = m->BaseVertex() * sizeof(StdVertex);
							rgn.dstOffset = vertexCount * sizeof(StdVertex);
							rgn.size = m->VertexCount() * sizeof(StdVertex);
							cpy.push_back(rgn);

							nodeBaseIndex += bvh->Nodes().size();
							vertexCount += m->VertexCount();
						}
					}
				}
			}
		}
		PROFILER_END;

		// Copy scene BVH
		PROFILER_BEGIN("Copy scene");
		fd.mBvhBase = (uint32_t)nodes.size();

		nodes.resize(nodes.size() + sceneBvh->Nodes().size());

		uint32_t leafNodeIndex = 0;

		for (uint32_t ni = 0; ni < sceneBvh->Nodes().size(); ni++){
			const ObjectBvh2::Node& n = sceneBvh->Nodes()[ni];
			GpuBvhNode& gn = nodes[fd.mBvhBase + ni];
			gn.RightOffset = n.mRightOffset;
			gn.Min = n.mBounds.mMin;
			gn.Max = n.mBounds.mMax;
			gn.StartIndex = leafNodeIndex;
			gn.PrimitiveCount = 0;

			if (n.mRightOffset == 0) {
				for (uint32_t i = 0; i < n.mCount; i++) {
					MeshRenderer* mr = dynamic_cast<MeshRenderer*>(sceneBvh->GetObject(n.mStartIndex + i));
					if (mr && mr->Visible()) {
						leafNodes[leafNodeIndex].NodeToWorld = mr->ObjectToWorld();
						leafNodes[leafNodeIndex].WorldToNode = mr->WorldToObject();
						leafNodes[leafNodeIndex].RootIndex = fd.mMeshes.at(mr->Mesh());
						leafNodes[leafNodeIndex].MaterialIndex = materials.size();

						DisneyMaterial mat = {};
						mat.BaseColor = mr->PushConstant("Color").float4Value.rgb;
						mat.Emission = mr->PushConstant("Emission").float3Value;
						mat.Roughness = mr->PushConstant("Roughness").floatValue;
						mat.Metallic = mr->PushConstant("Metallic").floatValue;
						mat.ClearcoatGloss = 1;
						mat.Specular = .5f;
						mat.Transmission = 1 - mr->PushConstant("Color").float4Value.a;

						if (mat.Emission.r + mat.Emission.g + mat.Emission.b > 0) {
							uint2 r = triangleRanges.at(mr->Mesh());
							for (uint32_t j = r.x; j < r.y; j++)
								lights.push_back(uint4(j, materials.size(), leafNodeIndex, 0));
						}

						if (mr->mName == "SuzanneSuzanne") {
							mat.Subsurface = 1;
						}
						if (mr->mName == "ClearcoatClearcoat") {
							mat.Clearcoat = 1;
						}
						
						materials.push_back(mat);

						leafNodeIndex++;
						gn.PrimitiveCount++;
					}
				}
			}
		}

		fd.mLightCount = lights.size();
		PROFILER_END;

		PROFILER_BEGIN("Upload data");
		if (fd.mNodes && fd.mNodes->Size() < sizeof(GpuBvhNode) * nodes.size())
			safe_delete(fd.mNodes);
		if (!fd.mNodes) fd.mNodes = new Buffer("SceneBvh", mScene->Instance()->Device(), sizeof(GpuBvhNode) * nodes.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

		if (fd.mLeafNodes && fd.mLeafNodes->Size() < sizeof(GpuLeafNode) * leafNodes.size())
			safe_delete(fd.mLeafNodes);
		if (!fd.mLeafNodes) fd.mLeafNodes = new Buffer("LeafNodes", mScene->Instance()->Device(), sizeof(GpuLeafNode) * leafNodes.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

		if (fd.mTriangles && fd.mTriangles->Size() < sizeof(uint3) * triangles.size())
			safe_delete(fd.mTriangles);
		if (!fd.mTriangles) fd.mTriangles = new Buffer("Triangles", mScene->Instance()->Device(), sizeof(uint3) * triangles.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

		if (fd.mLights && fd.mLights->Size() < sizeof(uint4) * lights.size())
			safe_delete(fd.mLights);
		if (!fd.mLights) fd.mLights = new Buffer("Lights", mScene->Instance()->Device(), sizeof(uint4) * lights.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

		if (fd.mMaterials && fd.mMaterials->Size() < sizeof(DisneyMaterial) * materials.size())
			safe_delete(fd.mMaterials);
		if (!fd.mMaterials) fd.mMaterials = new Buffer("Materials", mScene->Instance()->Device(), sizeof(DisneyMaterial) * materials.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

		if (fd.mVertices && fd.mVertices->Size() < sizeof(StdVertex) * vertexCount)
			safe_delete(fd.mVertices);
		if (!fd.mVertices) fd.mVertices = new Buffer("Vertices", mScene->Instance()->Device(), sizeof(StdVertex) * vertexCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		
		fd.mNodes->Upload(nodes.data(), sizeof(GpuBvhNode) * nodes.size());
		fd.mLeafNodes->Upload(leafNodes.data(), sizeof(GpuLeafNode) * leafNodes.size());
		fd.mMaterials->Upload(materials.data(), sizeof(DisneyMaterial)* materials.size());
		fd.mLights->Upload(lights.data(), sizeof(uint3) * lights.size());
		fd.mTriangles->Upload(triangles.data(), sizeof(uint3) * triangles.size());
		
		for (auto p : vertexCopies)
			vkCmdCopyBuffer(*commandBuffer, *p.first, *fd.mVertices, p.second.size(), p.second.data());

		VkBufferMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.buffer = *fd.mVertices;
		barrier.size = fd.mVertices->Size();
		vkCmdPipelineBarrier(*commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0, nullptr,
			1, &barrier,
			0, nullptr);

		fd.mLastBuild = mScene->Instance()->FrameCount();
		PROFILER_END;
	}

public:
	inline int Priority() override { return 10000; }

	PLUGIN_EXPORT Raytracing() : mScene(nullptr), mFrameIndex(0) { mEnabled = true; }
	PLUGIN_EXPORT ~Raytracing() {
		for (uint32_t i = 0; i < mScene->Instance()->Device()->MaxFramesInFlight(); i++) {
			safe_delete(mFrameData[i].mPrimary);
			safe_delete(mFrameData[i].mMeta);
			safe_delete(mFrameData[i].mNodes);
			safe_delete(mFrameData[i].mLeafNodes);
			safe_delete(mFrameData[i].mVertices);
			safe_delete(mFrameData[i].mTriangles);
			safe_delete(mFrameData[i].mLights);
			safe_delete(mFrameData[i].mMaterials);
		}
		safe_delete_array(mFrameData);
		for (Object* obj : mObjects)
			mScene->RemoveObject(obj);
	}

	PLUGIN_EXPORT bool Init(Scene* scene) override {
		mScene = scene;

		mScene->Environment()->EnableCelestials(false);
		mScene->Environment()->EnableScattering(false);
		mScene->Environment()->AmbientLight(.3f);
	
		#pragma region load glTF
		string folder = "Assets/Models/";
		string file = "cornellbox.gltf";

		shared_ptr<Material> opaque = make_shared<Material>("PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
		opaque->EnableKeyword("TEXTURED");
		opaque->SetParameter("TextureST", float4(1, 1, 0, 0));

		shared_ptr<Material> alphaClip = make_shared<Material>("Cutout PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
		alphaClip->RenderQueue(5000);
		alphaClip->BlendMode(BLEND_MODE_ALPHA);
		alphaClip->CullMode(VK_CULL_MODE_NONE);
		alphaClip->EnableKeyword("TEXTURED");
		alphaClip->EnableKeyword("ALPHA_CLIP");
		alphaClip->EnableKeyword("TWO_SIDED");
		alphaClip->SetParameter("TextureST", float4(1, 1, 0, 0));

		shared_ptr<Material> alphaBlend = make_shared<Material>("Transparent PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
		alphaBlend->RenderQueue(5000);
		alphaBlend->BlendMode(BLEND_MODE_ALPHA);
		alphaBlend->CullMode(VK_CULL_MODE_NONE);
		alphaBlend->EnableKeyword("TEXTURED");
		alphaBlend->EnableKeyword("TWO_SIDED");
		alphaBlend->SetParameter("TextureST", float4(1, 1, 0, 0));

		shared_ptr<Material> curOpaque = nullptr;
		shared_ptr<Material> curClip = nullptr;
		shared_ptr<Material> curBlend = nullptr;

		uint32_t arraySize =
			mScene->AssetManager()->LoadShader("Shaders/pbr.stm")->GetGraphics(PASS_MAIN, { "TEXTURED" })->mDescriptorBindings.at("MainTextures").second.descriptorCount;

		uint32_t opaque_i = 0;
		uint32_t clip_i = 0;
		uint32_t blend_i = 0;

		auto matfunc = [&](Scene* scene, aiMaterial* aimaterial) {
			aiString alphaMode;
			if (aimaterial->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS) {
				if (alphaMode == aiString("MASK")) return alphaClip;
				if (alphaMode == aiString("BLEND")) return alphaBlend;
			}
			return opaque;
		};
		auto objfunc = [&](Scene* scene, Object* object, aiMaterial* aimaterial) {
			MeshRenderer* renderer = dynamic_cast<MeshRenderer*>(object);
			if (!renderer) return;

			Material* mat = renderer->Material();
			uint32_t i;

			if (mat == opaque.get()) {
				i = opaque_i;
				opaque_i++;
				if (opaque_i >= arraySize) curOpaque.reset();
				if (!curOpaque) {
					opaque_i = opaque_i % arraySize;
					curOpaque = make_shared<Material>("PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
					curOpaque->EnableKeyword("TEXTURED");
					curOpaque->SetParameter("TextureST", float4(1, 1, 0, 0));
				}
				renderer->Material(curOpaque);
				mat = curOpaque.get();

			} else if (mat == alphaClip.get()) {
				i = clip_i;
				clip_i++;
				if (clip_i >= arraySize) curClip.reset();
				if (!curClip) {
					clip_i = clip_i % arraySize;
					curClip = make_shared<Material>("Cutout PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
					curClip->RenderQueue(5000);
					curClip->BlendMode(BLEND_MODE_ALPHA);
					curClip->CullMode(VK_CULL_MODE_NONE);
					curClip->EnableKeyword("TEXTURED");
					curClip->EnableKeyword("ALPHA_CLIP");
					curClip->EnableKeyword("TWO_SIDED");
					curClip->SetParameter("TextureST", float4(1, 1, 0, 0));
				}
				renderer->Material(curClip);
				mat = curClip.get();

			} else if (mat == alphaBlend.get()) {
				i = blend_i;
				blend_i++;
				if (blend_i >= 64) curBlend.reset();
				if (!curBlend) {
					blend_i = blend_i % arraySize;
					curBlend = make_shared<Material>("Transparent PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
					curBlend->RenderQueue(5000);
					curBlend->BlendMode(BLEND_MODE_ALPHA);
					curBlend->CullMode(VK_CULL_MODE_NONE);
					curBlend->EnableKeyword("TEXTURED");
					curBlend->EnableKeyword("TWO_SIDED");
					curBlend->SetParameter("TextureST", float4(1, 1, 0, 0));
				}
				renderer->Material(curBlend);
				mat = curBlend.get();

			} else return;

			mat->PassMask((PassType)(PASS_RAYTRACE));

			aiColor3D emissiveColor(0);
			aiColor4D baseColor(1);
			float metallic = 1.f;
			float roughness = 1.f;
			aiString baseColorTexture, metalRoughTexture, normalTexture, emissiveTexture;

			if (aimaterial->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE, &baseColorTexture) == AI_SUCCESS && baseColorTexture.length) {
				mat->SetParameter("MainTextures", i, scene->AssetManager()->LoadTexture(folder + baseColorTexture.C_Str()));
				baseColor = aiColor4D(1);
			} else
				mat->SetParameter("MainTextures", i, scene->AssetManager()->LoadTexture("Assets/Textures/white.png"));

			if (aimaterial->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalRoughTexture) == AI_SUCCESS && metalRoughTexture.length)
				mat->SetParameter("MaskTextures", i, scene->AssetManager()->LoadTexture(folder + metalRoughTexture.C_Str(), false));
			else
				mat->SetParameter("MaskTextures", i, scene->AssetManager()->LoadTexture("Assets/Textures/mask.png", false));

			if (aimaterial->GetTexture(aiTextureType_NORMALS, 0, &normalTexture) == AI_SUCCESS && normalTexture.length)
				mat->SetParameter("NormalTextures", i, scene->AssetManager()->LoadTexture(folder + normalTexture.C_Str(), false));
			else
				mat->SetParameter("NormalTextures", i, scene->AssetManager()->LoadTexture("Assets/Textures/bump.png", false));

			aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, baseColor);
			aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, metallic);
			aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, roughness);
			aimaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);

			renderer->PushConstant("TextureIndex", i);
			renderer->PushConstant("Color", float4(baseColor.r, baseColor.g, baseColor.b, baseColor.a));
			renderer->PushConstant("Roughness", roughness);
			renderer->PushConstant("Metallic", metallic);
			renderer->PushConstant("Emission", float3(emissiveColor.r, emissiveColor.g, emissiveColor.b));
		};

		Object* root = mScene->LoadModelScene(folder + file, matfunc, objfunc, .6f, 1.f, .05f, .0015f);
		root->LocalRotation(quaternion(float3(0, PI / 2, 0)));
		queue<Object*> nodes;
		nodes.push(root);
		while (nodes.size()) {
			Object* o = nodes.front();
			nodes.pop();
			for (uint32_t i = 0; i < o->ChildCount(); i++)
				nodes.push(o->Child(i));

			mObjects.push_back(o);
			if (Light* l = dynamic_cast<Light*>(o)){
				if (l->Type() == LIGHT_TYPE_SUN){
					l->CascadeCount(1);
					l->ShadowDistance(30);
				}
			}
		}
		#pragma endregion

		mFrameData = new FrameData[mScene->Instance()->Device()->MaxFramesInFlight()];
		for (uint32_t i = 0; i < mScene->Instance()->Device()->MaxFramesInFlight(); i++) {
			mFrameData[i].mPrimary = nullptr;
			mFrameData[i].mMeta = nullptr;
			mFrameData[i].mNodes = nullptr;
			mFrameData[i].mLeafNodes = nullptr;
			mFrameData[i].mVertices = nullptr;
			mFrameData[i].mTriangles = nullptr;
			mFrameData[i].mLights = nullptr;
			mFrameData[i].mMaterials = nullptr;
			mFrameData[i].mBvhBase = 0;
			mFrameData[i].mLightCount = 0;
			mFrameData[i].mLastBuild = 0;
		}

		return true;
	}

	PLUGIN_EXPORT void PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override {
		if (pass != PASS_MAIN) return;

		FrameData& fd = mFrameData[commandBuffer->Device()->FrameContextIndex()];
		if (fd.mLastBuild <= mScene->LastBvhBuild())
			Build(commandBuffer, fd);

		VkPipelineStageFlags dstStage, srcStage;
		VkImageMemoryBarrier barriers[2];

		if (fd.mPrimary && (fd.mPrimary->Width() != camera->FramebufferWidth() || fd.mPrimary->Height() != camera->FramebufferHeight())) {
			safe_delete(fd.mPrimary);
			safe_delete(fd.mMeta);
		}
		
		if (!fd.mPrimary) {
			fd.mPrimary = new Texture("Raytrace Primary", mScene->Instance()->Device(), camera->FramebufferWidth(), camera->FramebufferHeight(), 1,
				VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
			fd.mMeta = new Texture("Raytrace Primary", mScene->Instance()->Device(), camera->FramebufferWidth(), camera->FramebufferHeight(), 1,
				VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		
			barriers[0] = fd.mPrimary->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, srcStage, dstStage);
			barriers[1] = fd.mMeta->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, srcStage, dstStage);
			vkCmdPipelineBarrier(*commandBuffer,
				srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				2, barriers);
		}

		#pragma region raytrace
		FrameData& pfd = mFrameData[(commandBuffer->Device()->FrameContextIndex() + (commandBuffer->Device()->MaxFramesInFlight()-1)) % commandBuffer->Device()->MaxFramesInFlight()];
		bool accum = pfd.mPrimary && pfd.mPrimary->Width() == fd.mPrimary->Width() && pfd.mPrimary->Height() == fd.mPrimary->Height();

		Shader* rt = mScene->AssetManager()->LoadShader("Shaders/raytrace.stm");
		ComputeShader* trace = accum ? rt->GetCompute("Raytrace", { "ACCUMULATE" }) : rt->GetCompute("Raytrace", {});
		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, trace->mPipeline);

		fd.mViewProjection = camera->ViewProjection();
		fd.mInvViewProjection = inverse(camera->ViewProjection());
		fd.mCameraPosition = camera->WorldPosition();
		float2 res(fd.mPrimary->Width(), fd.mPrimary->Height());
		float near = camera->Near();
		float far = camera->Far();
		uint32_t vs = sizeof(StdVertex);
		uint32_t is = sizeof(uint32_t);

		commandBuffer->PushConstant(trace, "LastViewProjection", &pfd.mViewProjection);
		commandBuffer->PushConstant(trace, "LastCameraPosition", &pfd.mCameraPosition);
		commandBuffer->PushConstant(trace, "InvViewProj", &fd.mInvViewProjection);
		commandBuffer->PushConstant(trace, "Resolution", &res);
		commandBuffer->PushConstant(trace, "Near", &near);
		commandBuffer->PushConstant(trace, "Far", &far);
		commandBuffer->PushConstant(trace, "CameraPosition", &fd.mCameraPosition);
		commandBuffer->PushConstant(trace, "VertexStride", &vs);
		commandBuffer->PushConstant(trace, "IndexStride", &is);
		commandBuffer->PushConstant(trace, "BvhRoot", &fd.mBvhBase);
		commandBuffer->PushConstant(trace, "FrameIndex", &mFrameIndex);
		commandBuffer->PushConstant(trace, "LightCount", &fd.mLightCount);

		DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("RT", trace->mDescriptorSetLayouts[0]);
		VkDeviceSize bufSize = AlignUp(sizeof(CameraBuffer), commandBuffer->Device()->Limits().minUniformBufferOffsetAlignment);
		ds->CreateStorageTextureDescriptor(fd.mPrimary, trace->mDescriptorBindings.at("OutputPrimary").second.binding);
		ds->CreateStorageTextureDescriptor(fd.mMeta, trace->mDescriptorBindings.at("OutputMeta").second.binding);
		if (accum) {
			ds->CreateSampledTextureDescriptor(pfd.mPrimary, trace->mDescriptorBindings.at("PreviousPrimary").second.binding, VK_IMAGE_LAYOUT_GENERAL);
			ds->CreateSampledTextureDescriptor(pfd.mMeta, trace->mDescriptorBindings.at("PreviousMeta").second.binding, VK_IMAGE_LAYOUT_GENERAL);
		}
		ds->CreateStorageBufferDescriptor(fd.mNodes, 0, fd.mNodes->Size(), trace->mDescriptorBindings.at("SceneBvh").second.binding);
		ds->CreateStorageBufferDescriptor(fd.mLeafNodes, 0, fd.mLeafNodes->Size(), trace->mDescriptorBindings.at("LeafNodes").second.binding);
		ds->CreateStorageBufferDescriptor(fd.mVertices, 0, fd.mVertices->Size(), trace->mDescriptorBindings.at("Vertices").second.binding);
		ds->CreateStorageBufferDescriptor(fd.mTriangles, 0, fd.mTriangles->Size(), trace->mDescriptorBindings.at("Triangles").second.binding);
		ds->CreateStorageBufferDescriptor(fd.mMaterials, 0, fd.mMaterials->Size(), trace->mDescriptorBindings.at("Materials").second.binding);
		ds->CreateStorageBufferDescriptor(fd.mLights, 0, fd.mLights->Size(), trace->mDescriptorBindings.at("Lights").second.binding);
		ds->CreateSampledTextureDescriptor(mScene->AssetManager()->LoadTexture("Assets/Textures/rgbanoise.png", false), trace->mDescriptorBindings.at("NoiseTex").second.binding, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		ds->FlushWrites();
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, trace->mPipelineLayout, 0, 1, *ds, 0, nullptr);

		vkCmdDispatch(*commandBuffer, (fd.mPrimary->Width() + 7) / 8, (fd.mPrimary->Height() + 7) / 8, 1);
		#pragma endregion

		barriers[0] = fd.mPrimary->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, srcStage, dstStage);
		vkCmdPipelineBarrier(*commandBuffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, barriers);

		mFrameIndex++;
	}

	PLUGIN_EXPORT void PreRenderScene(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override {
		if (pass != PASS_MAIN) return;

		GraphicsShader* shader = mScene->AssetManager()->LoadShader("Shaders/rtblit.stm")->GetGraphics(PASS_MAIN, {});
		if (!shader) return;

		VkPipelineLayout layout = commandBuffer->BindShader(shader, pass, nullptr);
		if (!layout) return;

		float4 st(1, 1, 0, 0);
		float4 tst(1, -1, 0, 1);
		float exposure = .4f;

		FrameData& fd = mFrameData[commandBuffer->Device()->FrameContextIndex()];

		commandBuffer->PushConstant(shader, "ScaleTranslate", &st);
		commandBuffer->PushConstant(shader, "TextureST", &tst);
		commandBuffer->PushConstant(shader, "Exposure", &exposure);
		DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("Blit", shader->mDescriptorSetLayouts[0]);
		ds->CreateSampledTextureDescriptor(fd.mPrimary, shader->mDescriptorBindings.at("Radiance").second.binding, VK_IMAGE_LAYOUT_GENERAL);
		ds->FlushWrites();
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, *ds, 0, nullptr);
		vkCmdDraw(*commandBuffer, 6, 1, 0, 0);
	}
};

ENGINE_PLUGIN(Raytracing)