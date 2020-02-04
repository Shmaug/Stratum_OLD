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

#define PASS_RAYTRACE (1 << 23)

#pragma pack(push)
#pragma pack(1)
struct GpuBvhNode {
	float3 Min;
	uint32_t StartIndex;
	float3 Max;
	uint32_t PrimitiveCount;
	uint32_t RightOffset; // 1st child is at node[index + 1], 2nd child is at node[index + mRightOffset]
	uint32_t Mask;
};
struct GpuLeafNode {
	float4x4 WorldToNode;
	uint32_t RootIndex;
};
#pragma pack(pop)

class Raytracing : public EnginePlugin {
private:
	vector<Object*> mObjects;
	Scene* mScene;

	struct FrameData {
		bool mDirty;
		Buffer* mNodes;
		Buffer* mLeafNodes;
		Buffer* mVertices;
		Buffer* mTriangles;
		uint32_t mBvhBase;
		unordered_map<Mesh*, uint32_t> mMeshes; // Mesh, RootIndex
	};
	FrameData* mFrameData;
	
	void CopyMeshBVH(Mesh* mesh, vector<GpuBvhNode>& nodes, vector<StdVertex> vertices, vector<uint3>& triangles) {
		uint32_t baseVertex = vertices.size();


		TriangleBvh2* bvh = mesh->BVH();
		std::queue<uint32_t> nodeQueue;
		nodeQueue.push(0);
		while (nodeQueue.size()) {
			uint32_t ni = nodeQueue.front();
			nodeQueue.pop();

			TriangleBvh2::Node n = bvh->GetNode(ni);

			nodes.push_back({});
			GpuBvhNode& gn = nodes.back();
			gn.Mask = ~0;
			gn.RightOffset = n.mRightOffset;
			gn.PrimitiveCount = n.mCount;
			gn.Min = n.mBounds.mMin;
			gn.Max = n.mBounds.mMax;

			if (n.mRightOffset == 0) {
				gn.StartIndex = triangles.size();
				for (uint32_t i = 0; i < gn.PrimitiveCount; i++)
					triangles.push_back(baseVertex + bvh->GetPrimitive(gn.StartIndex + i));
			} else {
				nodeQueue.push(ni + 1);
				nodeQueue.push(ni + n.mRightOffset);
			}
		}

	}

	void Build(FrameData& fd) {
		ObjectBvh2* sceneBvh = mScene->BVH();

		vector<GpuBvhNode> nodes;
		vector<GpuLeafNode> leafNodes;
		vector<StdVertex> vertices;
		vector<uint3> triangles;

		// Collect mesh BVHs
		std::queue<uint32_t> nodeQueue;
		nodeQueue.push(0);
		while (nodeQueue.size()) {
			uint32_t ni = nodeQueue.front();
			nodeQueue.pop();

			ObjectBvh2::Node n = sceneBvh->GetNode(ni);

			if (n.mRightOffset == 0) {
				for (uint32_t i = 0; i < n.mCount; i++) {
					MeshRenderer* mr = dynamic_cast<MeshRenderer*>(sceneBvh->GetObject(n.mStartIndex + i));
					if (mr && (mr->LayerMask() & PASS_RAYTRACE) != 0) {
						fd.mMeshes.emplace(mr->Mesh(), (uint32_t)nodes.size());
						CopyMeshBVH(mr->Mesh(), nodes, vertices, triangles);
					}
				}
			} else {
				nodeQueue.push(ni + 1);
				nodeQueue.push(ni + n.mRightOffset);
			}
		}

		// Collect scene BVH

		fd.mBvhBase = (uint32_t)nodes.size();

		nodeQueue.push(0);
		while (nodeQueue.size()) {
			uint32_t ni = nodeQueue.front();
			nodeQueue.pop();

			ObjectBvh2::Node n = sceneBvh->GetNode(ni);

			nodes.push_back({});
			GpuBvhNode& gn = nodes.back();
			gn.Mask = 0;
			gn.RightOffset = n.mRightOffset;
			gn.PrimitiveCount = 0;
			gn.Min = n.mBounds.mMin;
			gn.Max = n.mBounds.mMax;

			if (n.mRightOffset == 0) {
				gn.StartIndex = (uint32_t)leafNodes.size();
				for (uint32_t i = 0; i < n.mCount; i++) {
					MeshRenderer* mr = dynamic_cast<MeshRenderer*>(sceneBvh->GetObject(n.mStartIndex + i));
					if (mr && (mr->LayerMask() & PASS_RAYTRACE) != 0) {
						leafNodes.push_back({});
						GpuLeafNode& l = leafNodes.back();
						l.WorldToNode = mr->WorldToObject();
						l.RootIndex = fd.mMeshes.at(mr->Mesh());
						gn.Mask |= mr->LayerMask();
						if (gn.PrimitiveCount == 0) {
							gn.Max = mr->Bounds().mMax;
							gn.Min = mr->Bounds().mMin;
						} else {
							gn.Max = max(gn.Max, mr->Bounds().mMax);
							gn.Min = min(gn.Min, mr->Bounds().mMin);
						}
						gn.PrimitiveCount++;
					}
				}
			} else {
				nodeQueue.push(ni + 1);
				nodeQueue.push(ni + n.mRightOffset);
			}
		}

		fd.mDirty = false;
	}

public:
	PLUGIN_EXPORT Raytracing() : mScene(nullptr) { mEnabled = true; }
	PLUGIN_EXPORT ~Raytracing() {
		for (uint32_t i = 0; i < mScene->Instance()->Device()->MaxFramesInFlight(); i++) {
			safe_delete(mFrameData[i].mNodes);
			safe_delete(mFrameData[i].mLeafNodes);
			safe_delete(mFrameData[i].mVertices);
			safe_delete(mFrameData[i].mTriangles);
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
		//mScene->Environment()->EnvironmentTexture(mScene->AssetManager()->LoadTexture("Assets/Textures/old_outdoor_theater_4k.hdr"));
	
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

			mat->PassMask((PassType)PASS_RAYTRACE);

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

		queue<Object*> nodes;
		nodes.push(mScene->LoadModelScene(folder + file, matfunc, objfunc, .6f, 1.f, .05f, .0015f));
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
		memset(mFrameData, 0, sizeof(FrameData)* mScene->Instance()->Device()->MaxFramesInFlight());
		for (uint32_t i = 0; i < mScene->Instance()->Device()->MaxFramesInFlight(); i++)
			mFrameData[i].mDirty = true;

		for (Camera* c : mScene->Cameras())
			c->SampleCount(VK_SAMPLE_COUNT_1_BIT);
	
		return true;
	}

	PLUGIN_EXPORT void PostProcess(CommandBuffer* commandBuffer, Camera* camera) override {
		Shader* rt = mScene->AssetManager()->LoadShader("Shaders/raytrace.stm");
		
		FrameData& fd = mFrameData[commandBuffer->Device()->FrameContextIndex()];
		if (fd.mDirty) Build(fd);

		ComputeShader* trace = rt->GetCompute("Raytrace", {});
		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, trace->mPipeline);
		float2 res(camera->ResolveBuffer()->Width(), camera->ResolveBuffer()->Height());
		float4x4 vp = inverse(camera->ViewProjection());
		float near = camera->Near();
		float far = camera->Far();
		float3 cp = camera->WorldPosition();
		uint32_t vs = sizeof(StdVertex);

		commandBuffer->PushConstant(trace, "Resolution", &res);
		commandBuffer->PushConstant(trace, "InvVP", &vp);
		commandBuffer->PushConstant(trace, "Near", &near);
		commandBuffer->PushConstant(trace, "Far", &far);
		commandBuffer->PushConstant(trace, "CameraPosition", &cp);
		commandBuffer->PushConstant(trace, "VertexStride", &vs);
		commandBuffer->PushConstant(trace, "BvhRoot", &fd.mBvhBase);

		DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("RT", trace->mDescriptorSetLayouts[0]);
		VkDeviceSize bufSize = AlignUp(sizeof(CameraBuffer), commandBuffer->Device()->Limits().minUniformBufferOffsetAlignment);
		ds->CreateUniformBufferDescriptor(camera->UniformBuffer(), bufSize * commandBuffer->Device()->FrameContextIndex(), bufSize, trace->mDescriptorBindings.at("Camera").second.binding);
		ds->CreateStorageTextureDescriptor(camera->ResolveBuffer(), trace->mDescriptorBindings.at("OutputTexture").second.binding);
		ds->CreateStorageBufferDescriptor(fd.mNodes, 0, fd.mNodes->Size(), trace->mDescriptorBindings.at("SceneBvh").second.binding);
		ds->CreateStorageBufferDescriptor(fd.mLeafNodes, 0, fd.mLeafNodes->Size(), trace->mDescriptorBindings.at("LeafNodes").second.binding);
		ds->CreateStorageBufferDescriptor(fd.mVertices, 0, fd.mVertices->Size(), trace->mDescriptorBindings.at("Vertices").second.binding);
		ds->CreateStorageBufferDescriptor(fd.mTriangles, 0, fd.mTriangles->Size(), trace->mDescriptorBindings.at("Triangles").second.binding);
		ds->FlushWrites();

		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, trace->mPipelineLayout, 0, 1, *ds, 0, nullptr);
		vkCmdDispatch(*commandBuffer, (camera->ResolveBuffer()->Width() + 7) / 8, (camera->ResolveBuffer()->Height() + 7) / 8, 1);
	}

};

ENGINE_PLUGIN(Raytracing)