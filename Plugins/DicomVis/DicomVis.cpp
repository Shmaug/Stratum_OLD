#include <Scene/MeshRenderer.hpp>
#include <Scene/TextRenderer.hpp>
#include <Util/Profiler.hpp>
#include <Scene/Interface.hpp>

#include <Core/EnginePlugin.hpp>
#include <assimp/pbrmaterial.h>

using namespace std;

class DicomVis : public EnginePlugin {
private:
	Scene* mScene;
	vector<Object*> mObjects;
	Object* mSelected;

	float3 mCameraEuler;

	Object* mPlayer;
	Camera* mMainCamera;

	float3 mPlayerVelocity;
	bool mFlying;

	MouseKeyboardInput* mInput;

	bool mShowPerformance;
	bool mSnapshotPerformance;
	ProfilerSample mProfilerFrames[PROFILER_FRAME_COUNT - 1];
	uint32_t mSelectedFrame;

	float mFrameTimeAccum;
	float mFps;
	uint32_t mFrameCount;

public:
	PLUGIN_EXPORT DicomVis(): mScene(nullptr), mSelected(nullptr), mFlying(false), mShowPerformance(false), mSnapshotPerformance(false), mCameraEuler(0), mFrameCount(0), mFrameTimeAccum(0), mFps(0), mPlayerVelocity(0) {
		mEnabled = true;
	}
	PLUGIN_EXPORT ~DicomVis() {
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
}

	PLUGIN_EXPORT bool Init(Scene* scene) override {
		mScene = scene;
		mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

		shared_ptr<Object> player = make_shared<Object>("Player");
		mScene->AddObject(player);
		mPlayer = player.get();
		mObjects.push_back(mPlayer);

		shared_ptr<Camera> camera = make_shared<Camera>("Camera", mScene->Instance()->Window());
		mScene->AddObject(camera);
		camera->Near(.01f);
		camera->Far(800.f);
		camera->FieldOfView(radians(65.f));
		camera->LocalPosition(0, 1.6f, 0);
		mMainCamera = camera.get();
		mPlayer->AddChild(mMainCamera);
		mObjects.push_back(mMainCamera);

		#pragma region load glTF
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

		string folder = "Assets/Models/room/";
		string file = "CrohnsProtoRoom.gltf";

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

			}
			else if (mat == alphaClip.get()) {
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

			}
			else if (mat == alphaBlend.get()) {
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

			}
			else return;

			aiColor3D emissiveColor(0);
			aiColor4D baseColor(1);
			float metallic = 1.f;
			float roughness = 1.f;
			aiString baseColorTexture, metalRoughTexture, normalTexture, emissiveTexture;

			if (aimaterial->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE, &baseColorTexture) == AI_SUCCESS && baseColorTexture.length) {
				mat->SetParameter("MainTextures", i, scene->AssetManager()->LoadTexture(folder + baseColorTexture.C_Str()));
				baseColor = aiColor4D(1);
			}
			else
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
			mObjects.push_back(o);
			for (uint32_t i = 0; i < o->ChildCount(); i++)
				nodes.push(o->Child(i));

			if (Light* l = dynamic_cast<Light*>(o))
				l->CascadeCount(1);
		}
		#pragma endregion

		mScene->Environment()->EnableCelestials(false);
		mScene->Environment()->EnableScattering(false);
		mScene->Environment()->AmbientLight(.2f);
		mScene->Environment()->EnvironmentTexture(mScene->AssetManager()->LoadTexture("Assets/Textures/ocean hdri.jpg"));

		return true;
	}
	PLUGIN_EXPORT void Update() override {
		#pragma region player movement
		#pragma region rotate camera
		if (mInput->LockMouse()) {
			float3 md = float3(mInput->CursorDelta(), 0);
			md = float3(md.y, md.x, 0) * .003f;
			mCameraEuler += md;
			mCameraEuler.x = clamp(mCameraEuler.x, -PI * .5f, PI * .5f);
			mMainCamera->LocalRotation(quaternion(float3(mCameraEuler.x, 0, 0)));
			mPlayer->LocalRotation(quaternion(float3(0, mCameraEuler.y, 0)));
		}
		#pragma endregion

		#pragma region apply movement force
		float3 move = 0;
		if (mInput->KeyDown(KEY_W)) move.z += 1;
		if (mInput->KeyDown(KEY_S)) move.z -= 1;
		if (mInput->KeyDown(KEY_D)) move.x += 1;
		if (mInput->KeyDown(KEY_A)) move.x -= 1;
		move = (mFlying ? mMainCamera->WorldRotation() : mPlayer->WorldRotation()) * move;
		if (dot(move, move) > .001f) {
			move = normalize(move);
			move *= 2.5f;
			if (mInput->KeyDown(KEY_LSHIFT))
				move *= 2.5f;
		}
		float3 mf = 5 * (move - mPlayerVelocity) * mScene->Instance()->DeltaTime();
		if (!mFlying) mf.y = 0;
		mPlayerVelocity += mf;
		#pragma endregion

		if (!mFlying) mPlayerVelocity.y -= 9.8f * mScene->Instance()->DeltaTime(); // gravity

		float3 p = mPlayer->WorldPosition();
		p += mPlayerVelocity * mScene->Instance()->DeltaTime(); // integrate velocity

		if (p.y < .5f) {
			p.y = .5f;
			mPlayerVelocity.y = 0;

			if (!mFlying && mInput->KeyDown(KEY_SPACE))
				mPlayerVelocity.y += 4.f;

		}
		mPlayer->LocalPosition(p);
		#pragma endregion

		if (mInput->KeyDownFirst(MOUSE_RIGHT))
			mInput->LockMouse(!mInput->LockMouse());

		if (mInput->KeyDownFirst(KEY_F1))
			mScene->DrawGizmos(!mScene->DrawGizmos());
		if (mInput->KeyDownFirst(KEY_F2))
			mFlying = !mFlying;


		if (mInput->KeyDownFirst(KEY_TILDE))
			mShowPerformance = !mShowPerformance;

		// Snapshot profiler frames
		if (mInput->KeyDownFirst(KEY_F3)) {
			mSnapshotPerformance = !mSnapshotPerformance;
			if (mSnapshotPerformance) {
				mSelectedFrame = PROFILER_FRAME_COUNT;
				queue<pair<ProfilerSample*, const ProfilerSample*>> samples;
				for (uint32_t i = 0; i < PROFILER_FRAME_COUNT - 1; i++) {
					mProfilerFrames[i].mParent = nullptr;
					samples.push(make_pair(mProfilerFrames + i, Profiler::Frames() + ((i + Profiler::CurrentFrameIndex() + 2) % PROFILER_FRAME_COUNT)));
					while (samples.size()) {
						auto p = samples.front();
						samples.pop();

						p.first->mStartTime = p.second->mStartTime;
						p.first->mDuration = p.second->mDuration;
						strncpy(p.first->mLabel, p.second->mLabel, PROFILER_LABEL_SIZE);
						p.first->mChildren.resize(p.second->mChildren.size());

						auto it2 = p.second->mChildren.begin();
						for (auto it = p.first->mChildren.begin(); it != p.first->mChildren.end(); it++, it2++) {
							it->mParent = p.first;
							samples.push(make_pair(&*it, &*it2));
						}
					}
				}
			}
		}

		// count fps
		mFrameTimeAccum += mScene->Instance()->DeltaTime();
		mFrameCount++;
		if (mFrameTimeAccum > 1.f) {
			mFps = mFrameCount / mFrameTimeAccum;
			mFrameTimeAccum -= 1.f;
			mFrameCount = 0;
		}
	}

	PLUGIN_EXPORT void PostRenderScene(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override {
		if (pass != PASS_MAIN || camera != mScene->Cameras()[0]) return;
		if (mShowPerformance) {
			char tmpText[64];

			Font* sem11 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 11);
			Font* sem16 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 16);
			Font* reg14 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Regular.ttf", 14);

			float2 s(camera->FramebufferWidth(), camera->FramebufferHeight());

			float graphHeight = 100;

			#ifdef PROFILER_ENABLE
			const uint32_t pointCount = PROFILER_FRAME_COUNT - 1;

			float2 points[pointCount];
			float m = 0;
			for (uint32_t i = 0; i < pointCount; i++) {
				points[i].y = (mSnapshotPerformance ? mProfilerFrames[i] : Profiler::Frames()[(i + Profiler::CurrentFrameIndex() + 2) % PROFILER_FRAME_COUNT]).mDuration.count() * 1e-6f;
				points[i].x = (float)i / (pointCount - 1.f);
				m = fmaxf(points[i].y, m);
			}
			m = fmaxf(m, 5.f) + 3.f;
			for (uint32_t i = 0; i < pointCount; i++)
				points[i].y /= m;

			DrawScreenRect(commandBuffer, camera, float2(0, 0), float2(s.x, graphHeight), float4(.1f, .1f, .1f, 1));
			DrawScreenRect(commandBuffer, camera, float2(0, graphHeight - 1), float2(s.x, 2), float4(.2f, .2f, .2f, 1));

			snprintf(tmpText, 64, "%.1fms", m);
			sem11->DrawScreenString(commandBuffer, camera, tmpText, float4(.6f, .6f, .6f, 1.f), float2(2, graphHeight - 10), 11.f);

			for (float i = 1; i < 3; i++) {
				float x = m * i / 3.f;
				snprintf(tmpText, 32, "%.1fms", x);
				DrawScreenRect(commandBuffer, camera, float2(0, graphHeight * (i / 3.f) - 1), float2(s.x, 1), float4(.2f, .2f, .2f, 1));
				sem11->DrawScreenString(commandBuffer, camera, tmpText, float4(.6f, .6f, .6f, 1.f), float2(2, graphHeight * (i / 3.f) + 2), 11.f);
			}

			DrawScreenLine(commandBuffer, camera, points, pointCount, 0, float2(s.x, graphHeight), float4(.2f, 1.f, .2f, 1.f));

			if (mSnapshotPerformance) {
				float2 c = mInput->CursorPos();
				c.y = s.y - c.y;

				if (c.y < 100) {
					uint32_t hvr = (uint32_t)((c.x / s.x) * (PROFILER_FRAME_COUNT - 2) + .5f);
					DrawScreenRect(commandBuffer, camera, float2(s.x * hvr / (PROFILER_FRAME_COUNT - 2), 0), float2(1, graphHeight), float4(1, 1, 1, .15f));
					if (mInput->KeyDown(MOUSE_LEFT))
						mSelectedFrame = hvr;
				}

				if (mSelectedFrame < PROFILER_FRAME_COUNT - 1) {
					ProfilerSample* selected = nullptr;
					float sampleHeight = 20;

					// selection line
					DrawScreenRect(commandBuffer, camera, float2(s.x * mSelectedFrame / (PROFILER_FRAME_COUNT - 2), 0), float2(1, graphHeight), 1);

					float id = 1.f / (float)mProfilerFrames[mSelectedFrame].mDuration.count();

					queue<pair<ProfilerSample*, uint32_t>> samples;
					samples.push(make_pair(mProfilerFrames + mSelectedFrame, 0));
					while (samples.size()) {
						auto p = samples.front();
						samples.pop();

						float2 pos(s.x * (p.first->mStartTime - mProfilerFrames[mSelectedFrame].mStartTime).count() * id, graphHeight + 20 + sampleHeight * p.second);
						float2 size(s.x * (float)p.first->mDuration.count() * id, sampleHeight);
						float4 col(0, 0, 0, 1);

						if (c.x > pos.x&& c.y > pos.y&& c.x < pos.x + size.x && c.y < pos.y + size.y) {
							selected = p.first;
							col.rgb = 1;
						}

						DrawScreenRect(commandBuffer, camera, pos, size, col);
						DrawScreenRect(commandBuffer, camera, pos + 1, size - 2, float4(.3f, .9f, .3f, 1));

						for (auto it = p.first->mChildren.begin(); it != p.first->mChildren.end(); it++)
							samples.push(make_pair(&*it, p.second + 1));
					}

					if (selected) {
						snprintf(tmpText, 64, "%s: %.2fms\n", selected->mLabel, selected->mDuration.count() * 1e-6f);
						DrawScreenRect(commandBuffer, camera, float2(0, graphHeight), float2(s.x, 20), float4(0, 0, 0, .8f));
						reg14->DrawScreenString(commandBuffer, camera, tmpText, 1, float2(s.x * .5f, graphHeight + 8), 14.f, TEXT_ANCHOR_MID, TEXT_ANCHOR_MID);
					}
				}

			}
			#endif

			snprintf(tmpText, 64, "%.2f fps | %llu tris\n", mFps, commandBuffer->mTriangleCount);
			sem16->DrawScreenString(commandBuffer, camera, tmpText, 1.f, float2(5, camera->FramebufferHeight() - 18), 18.f);
		}
	}
	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) override {
		float2 s(camera->FramebufferWidth(), camera->FramebufferHeight());
		float2 c = mInput->CursorPos();
		Ray ray = camera->ScreenToWorldRay(c / s);
		float t;
		PROFILER_BEGIN("Raycast");
		if (mScene->Raycast(ray, &t))
			Gizmos::DrawWireSphere(ray.mOrigin + ray.mDirection * t, .02f, 1);
		PROFILER_END;
	}
};

ENGINE_PLUGIN(DicomVis)