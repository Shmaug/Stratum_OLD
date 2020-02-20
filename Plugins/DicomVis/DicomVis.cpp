#include <Scene/MeshRenderer.hpp>
#include <Content/Font.hpp>
#include <Scene/GUI.hpp>
#include <Util/Profiler.hpp>

#include <Core/EnginePlugin.hpp>
#include <assimp/pbrmaterial.h>

#include "Dicom.hpp"

using namespace std;

class DicomVis : public EnginePlugin {
private:
	Scene* mScene;
	vector<Object*> mObjects;
	Object* mSelected;

	uint32_t mFrameIndex;

	float3 mVolumePosition;
	quaternion mVolumeRotation;
	float3 mVolumeScale;

	bool mLighting;
	bool mPhysicalShading;
	bool mColorize;
	bool mInvert;
	float mLightStepSize;
	float mStepSize;
	float mDensity;
	float mRemapMin;
	float mRemapMax;
	float mCutoff;
	float mVolumeScatter;
	float mVolumeExtinction;
	float mVolumePhaseHG;

	Texture* mRawVolume;
	Texture* mRawMask;
	bool mRawVolumeNew;
	bool mRawMaskNew;
	bool mVolumeColored;
	
	struct FrameData {
		Texture* mBakedVolume;
		Texture* mOpticalDensity;
		bool mImagesNew;
		bool mBakeDirty;
		bool mLightingDirty;
	};
	FrameData* mFrameData;

	Camera* mMainCamera;

	MouseKeyboardInput* mInput;

	float mZoom;
	

	bool mShowPerformance;
	bool mSnapshotPerformance;
	ProfilerSample mProfilerFrames[PROFILER_FRAME_COUNT - 1];
	uint32_t mSelectedFrame;

	float mFrameTimeAccum;
	float mFps;
	uint32_t mFrameCount;

	std::unordered_map<std::string, bool> mDataFolders;

public:
	PLUGIN_EXPORT DicomVis(): mScene(nullptr), mSelected(nullptr), mShowPerformance(false), mSnapshotPerformance(false),
		mFrameCount(0), mFrameTimeAccum(0), mFps(0), mFrameIndex(0), mRawVolume(nullptr), mRawMask(nullptr), mRawMaskNew(false), mRawVolumeNew(false), mColorize(false),
		mPhysicalShading(false), mLighting(false),
		mDensity(100.f), mRemapMin(.125f), mRemapMax(1.f), mCutoff(1.f), mStepSize(.001f), mLightStepSize(.005f),
		mVolumeScatter(100.f), mVolumeExtinction(20.f) {
		mEnabled = true;
	}
	PLUGIN_EXPORT ~DicomVis() {
		safe_delete(mRawVolume);
		for (uint32_t i = 0; i < mScene->Instance()->Device()->MaxFramesInFlight(); i++) {
			safe_delete(mFrameData[i].mBakedVolume);
			safe_delete(mFrameData[i].mOpticalDensity);
		}
		for (Object* obj : mObjects)
			mScene->RemoveObject(obj);
	}

	PLUGIN_EXPORT bool Init(Scene* scene) override {
		mScene = scene;
		mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

		mZoom = 3.f;

		shared_ptr<Camera> camera = make_shared<Camera>("Camera", mScene->Instance()->Window());
		mScene->AddObject(camera);
		camera->Near(.01f);
		camera->Far(800.f);
		camera->FieldOfView(radians(65.f));
		camera->LocalPosition(0, 1.6f, -mZoom);
		mMainCamera = camera.get();
		mObjects.push_back(mMainCamera);

		#pragma region plane
		auto planeMat = make_shared<Material>("Plane", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
		planeMat->EnableKeyword("TEXTURED");
		planeMat->SetParameter("MainTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/grid.png"));
		planeMat->SetParameter("NormalTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/bump.png"));
		planeMat->SetParameter("MaskTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/mask.png"));
		planeMat->SetParameter("TextureST", float4(256, 256, 1, 1));
		planeMat->SetParameter("Color", float4(1));
		planeMat->SetParameter("Metallic", 0.f);
		planeMat->SetParameter("Roughness", .5f);
		planeMat->SetParameter("BumpStrength", 1.f);
		planeMat->SetParameter("Emission", float3(0));

		auto plane = make_shared<MeshRenderer>("Plane");
		plane->Mesh(shared_ptr<Mesh>(Mesh::CreatePlane("Plane", mScene->Instance()->Device(), 512.f)));
		plane->Material(planeMat);
		plane->PushConstant("TextureIndex", 0u);
		plane->LocalRotation(quaternion(float3(-PI / 2, 0, 0)));
		mScene->AddObject(plane);
		mObjects.push_back(plane.get());
		#pragma endregion

		mScene->Environment()->EnableCelestials(false);
		mScene->Environment()->EnableScattering(false);
		mScene->Environment()->AmbientLight(.6f);

		string path = "/Data";
		for (uint32_t i = 0; i < mScene->Instance()->CommandLineArguments().size(); i++)
			if (mScene->Instance()->CommandLineArguments()[i] == "--datapath") {
				i++;
				if (i < mScene->Instance()->CommandLineArguments().size())
					path = mScene->Instance()->CommandLineArguments()[i];
			}
		if (!fs::exists(path)) path = "/data";
		if (!fs::exists(path)) path = "~/Data";
		if (!fs::exists(path)) path = "~/data";
		if (!fs::exists(path)) path = "C:/Data";
		if (!fs::exists(path)) path = "D:/Data";
		if (!fs::exists(path)) path = "E:/Data";
		if (!fs::exists(path)) path = "F:/Data";
		if (!fs::exists(path)) path = "G:/Data";

		for (const auto& p : fs::recursive_directory_iterator(path))
			if (p.path().extension().string() == ".dcm")
				mDataFolders[p.path().parent_path().string()] = false;
			else if (p.path().extension().string() == ".raw")
				mDataFolders[p.path().parent_path().string()] = true;

		mFrameData = new FrameData[mScene->Instance()->Device()->MaxFramesInFlight()];
		memset(mFrameData, 0, sizeof(FrameData) * mScene->Instance()->Device()->MaxFramesInFlight());

		return true;
	}
	PLUGIN_EXPORT void Update() override {
		if (mInput->KeyDownFirst(KEY_F1))
			mScene->DrawGizmos(!mScene->DrawGizmos());
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

		if (mInput->GetPointer(0)->mLastGuiHitT < 0) {
			mZoom = clamp(mZoom - mInput->ScrollDelta() * .05f, -1.f, 5.f);
			mMainCamera->LocalPosition(0, 1.6f, -mZoom);

			if (mInput->KeyDown(MOUSE_LEFT)) {
				float3 axis = mMainCamera->WorldRotation() * float3(0, 1, 0) * mInput->CursorDelta().x + mMainCamera->WorldRotation() * float3(1, 0, 0) * mInput->CursorDelta().y;
				if (dot(axis, axis) > .001f){
					mVolumeRotation = quaternion(length(axis) * .003f, -normalize(axis)) * mVolumeRotation;

					for (uint32_t i = 0; i < mScene->Instance()->Device()->MaxFramesInFlight(); i++)
						mFrameData[i].mLightingDirty = true;
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

	inline void MarkDirty(bool bake, bool lighting) {
		for (uint32_t i = 0; i < mScene->Instance()->Device()->MaxFramesInFlight(); i++) {
			mFrameData[i].mBakeDirty = mFrameData[i].mBakeDirty || bake;
			mFrameData[i].mLightingDirty = mFrameData[i].mLightingDirty || lighting;
		}
	}

	PLUGIN_EXPORT void PreRenderScene(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override {
		if (pass != PASS_MAIN || camera != mScene->Cameras()[0]) return;
		Font* reg14 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Regular.ttf", 14);
		Font* sem11 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 11);
		Font* sem16 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 16);
		Font* bld24 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Bold.ttf", 24);

		float2 s(camera->FramebufferWidth(), camera->FramebufferHeight());
		float2 c = mInput->CursorPos();
		c.y = s.y - c.y;
		
		if (mShowPerformance) {
			char tmpText[64];
			snprintf(tmpText, 64, "%.2f fps\n", mFps);
			GUI::DrawString(sem16, tmpText, 1.f, float2(5, camera->FramebufferHeight() - 18), 18.f, TEXT_ANCHOR_MIN, TEXT_ANCHOR_MAX);

			#ifdef PROFILER_ENABLE
			const uint32_t pointCount = PROFILER_FRAME_COUNT - 1;
			
			float graphHeight = 100;

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

			GUI::Rect(fRect2D(0, 0, s.x, graphHeight), float4(.1f, .1f, .1f, 1));
			GUI::Rect(fRect2D(0, graphHeight - 1, s.x, 2), float4(.2f, .2f, .2f, 1));

			snprintf(tmpText, 64, "%.1fms", m);
			GUI::DrawString(sem11, tmpText, float4(.6f, .6f, .6f, 1.f), float2(2, graphHeight - 10), 11.f);

			for (float i = 1; i < 3; i++) {
				float x = m * i / 3.f;
				snprintf(tmpText, 32, "%.1fms", x);
				GUI::Rect(fRect2D(0, graphHeight * (i / 3.f) - 1, s.x, 1), float4(.2f, .2f, .2f, 1));
				GUI::DrawString(sem11, tmpText, float4(.6f, .6f, .6f, 1.f), float2(2, graphHeight * (i / 3.f) + 2), 11.f);
			}

			GUI::DrawScreenLine(points, pointCount, 1.5f, 0, float2(s.x, graphHeight), float4(.2f, 1.f, .2f, 1.f));

			if (mSnapshotPerformance) {
				if (c.y < 100) {
					uint32_t hvr = (uint32_t)((c.x / s.x) * (PROFILER_FRAME_COUNT - 2) + .5f);
					GUI::Rect(fRect2D(s.x * hvr / (PROFILER_FRAME_COUNT - 2), 0, 1, graphHeight), float4(1, 1, 1, .15f));
					if (mInput->KeyDown(MOUSE_LEFT))
						mSelectedFrame = hvr;
				}

				if (mSelectedFrame < PROFILER_FRAME_COUNT - 1) {
					ProfilerSample* selected = nullptr;
					float sampleHeight = 20;

					// selection line
					GUI::Rect(fRect2D(s.x * mSelectedFrame / (PROFILER_FRAME_COUNT - 2), 0, 1, graphHeight), 1);

					float id = 1.f / (float)mProfilerFrames[mSelectedFrame].mDuration.count();

					queue<pair<ProfilerSample*, uint32_t>> samples;
					samples.push(make_pair(mProfilerFrames + mSelectedFrame, 0));
					while (samples.size()) {
						auto p = samples.front();
						samples.pop();

						float2 pos(s.x * (p.first->mStartTime - mProfilerFrames[mSelectedFrame].mStartTime).count() * id, graphHeight + 20 + sampleHeight * p.second);
						float2 size(s.x * (float)p.first->mDuration.count() * id, sampleHeight);
						float4 col(0, 0, 0, 1);

						if (c.x > pos.x&& c.y > pos.y && c.x < pos.x + size.x && c.y < pos.y + size.y) {
							selected = p.first;
							col.rgb = 1;
						}

						GUI::Rect(fRect2D(pos, size), col);
						GUI::Rect(fRect2D(pos + 1, size - 2), float4(.3f, .9f, .3f, 1));

						for (auto it = p.first->mChildren.begin(); it != p.first->mChildren.end(); it++)
							samples.push(make_pair(&*it, p.second + 1));
					}

					if (selected) {
						snprintf(tmpText, 64, "%s: %.2fms\n", selected->mLabel, selected->mDuration.count() * 1e-6f);
						GUI::Rect(fRect2D(0, graphHeight, s.x, 20), float4(0,0,0,.8f));
						GUI::DrawString(reg14, tmpText, 1, float2(s.x * .5f, graphHeight + 8), 14.f, TEXT_ANCHOR_MID, TEXT_ANCHOR_MID);
					}
				}

			}
			#endif
		}

		GUI::BeginScreenLayout(LAYOUT_VERTICAL, fRect2D(10, s.y * .5f - 400, 300, 800), float4(.3f, .3f, .3f, 1), 10);

		GUI::LayoutLabel(bld24, "Load Data Set", 24, 30, 0, 1);

		GUI::LayoutSeparator(.5f, 1);

		GUI::BeginScrollSubLayout(200, mDataFolders.size() * 24, float4(.2f, .2f, .2f, 1), 5);
		for (const auto& p : mDataFolders)
			if (GUI::LayoutButton(sem16, fs::path(p.first).stem().string(), 16, 24, p.second ? float4(.4f, .4f, .15f, 1) : float4(.2f, .2f, .2f, 1), 1, 2, TEXT_ANCHOR_MID))
				LoadVolume(commandBuffer, p.first, p.second);
		GUI::EndLayout();

		if (GUI::LayoutButton(sem16, "Colorize", 16, 24, mColorize ? float4(.5f, .5f, .5f, 1) : float4(.25f, .25f, .25f, 1), 1)) {
			mColorize = !mColorize;
			MarkDirty(true, true);
		}
		if (GUI::LayoutButton(sem16, "Lighting", 16, 24, mLighting ? float4(.5f, .5f, .5f, 1) : float4(.25f, .25f, .25f, 1), 1)) {
			mLighting = !mLighting;
			MarkDirty(false, true);
		}
		if (GUI::LayoutButton(sem16, "Physical Shading", 16, 24, mPhysicalShading ? float4(.5f, .5f, .5f, 1) : float4(.25f, .25f, .25f, 1), 1)) {
			mPhysicalShading = !mPhysicalShading;
			MarkDirty(false, true);
		}
		if (GUI::LayoutButton(sem16, "Invert", 16, 24, mInvert ? float4(.5f, .5f, .5f, 1) : float4(.25f, .25f, .25f, 1), 1)) {
			mInvert = !mInvert;
			MarkDirty(true, true);
		}

		GUI::LayoutSeparator(.5f, 1, 3);

		GUI::LayoutLabel(bld24, "Render Settings", 18, 24, 0, 1);

		GUI::LayoutSpace(8);

		GUI::LayoutLabel(sem16, "Step Size: " + to_string(mStepSize), 16, 16, 0, 1, 0, TEXT_ANCHOR_MIN);
		if (GUI::LayoutSlider(mStepSize, .0005f, .01f, 16, float4(.5f, .5f, .5f, 1), 4)) MarkDirty(false, true);

		GUI::LayoutLabel(sem16, "Light Step Size: " + to_string(mLightStepSize), 16, 16, 0, 1, 0, TEXT_ANCHOR_MIN);
		if (GUI::LayoutSlider(mLightStepSize, .0005f, .01f, 16, float4(.5f, .5f, .5f, 1), 4)) MarkDirty(false, true);

		GUI::LayoutSpace(5);

		GUI::LayoutLabel(sem16, "Density: " + to_string(mDensity), 16, 16, 0, 1, 0, TEXT_ANCHOR_MIN);
		if (GUI::LayoutSlider(mDensity, 10, 2000.f, 16, float4(.5f, .5f, .5f, 1), 4)) MarkDirty(false, true);

		GUI::LayoutLabel(sem16, "Scattering: " + to_string(mVolumeScatter), 16, 16, 0, 1, 0, TEXT_ANCHOR_MIN);
		if (GUI::LayoutSlider(mVolumeScatter, 0, 150, 16, float4(.5f, .5f, .5f, 1), 4));

		GUI::LayoutLabel(sem16, "Extinction: " + to_string(mVolumeExtinction), 16, 16, 0, 1, 0, TEXT_ANCHOR_MIN);
		if (GUI::LayoutSlider(mVolumeExtinction, 0, 80, 16, float4(.5f, .5f, .5f, 1), 4));

		GUI::LayoutLabel(sem16, "HG Phase: " + to_string(mVolumePhaseHG), 16, 16, 0, 1, 0, TEXT_ANCHOR_MIN);
		if (GUI::LayoutSlider(mVolumePhaseHG, -1, 1, 16, float4(.5f, .5f, .5f, 1), 4));

		GUI::LayoutSpace(5);

		GUI::LayoutLabel(sem16, "Remap Min: " + to_string(mRemapMin), 16, 16, 0, 1, 0, TEXT_ANCHOR_MIN);
		if (GUI::LayoutSlider(mRemapMin, 0, 1, 16, float4(.5f, .5f, .5f, 1), 4)) MarkDirty(true, true);

		GUI::LayoutLabel(sem16, "Remap Max: " + to_string(mRemapMax), 16, 16, 0, 1, 0, TEXT_ANCHOR_MIN);
		if (GUI::LayoutSlider(mRemapMax, 0, 1, 16, float4(.5f, .5f, .5f, 1), 4)) MarkDirty(true, true);

		GUI::LayoutLabel(sem16, "Cutoff: " + to_string(mCutoff), 16, 16, 0, 1, 0, TEXT_ANCHOR_MIN);
		if (GUI::LayoutSlider(mCutoff, 0, 1, 16, float4(.5f, .5f, .5f, 1), 4)) MarkDirty(true, true);

		GUI::EndLayout();
	}

	PLUGIN_EXPORT void PostProcess(CommandBuffer* commandBuffer, Camera* camera) override {
		if (!mRawVolume) return;

		if (mRawVolumeNew) {
			mRawVolume->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
			mRawVolumeNew = false;
		}
		if (mRawMaskNew) {
			mRawMask->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
			mRawMaskNew = false;
		}

		FrameData& fd = mFrameData[commandBuffer->Device()->FrameContextIndex()];
		if (fd.mImagesNew) {
			fd.mBakedVolume->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
			fd.mOpticalDensity->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
			fd.mImagesNew = false;
		}
		
		float2 res(camera->FramebufferWidth(), camera->FramebufferHeight());
		float4x4 ivp[2];
		ivp[0] = camera->InverseViewProjection(EYE_LEFT);
		ivp[1] = camera->InverseViewProjection(EYE_RIGHT);
		float3 cp[2];
		cp[0] = camera->InverseView(EYE_LEFT)[3].xyz;
		cp[1] = camera->InverseView(EYE_RIGHT)[3].xyz;
		float4 ivr = inverse(mVolumeRotation).xyzw;
		float3 ivs = 1.f / mVolumeScale;
		float near = camera->Near();
		float far = camera->Far();
		
		float remapRange = 1.f / (mRemapMax - mRemapMin);

		float3 lightCol = 2;
		float3 lightDir = normalize(float3(.1f, .5f, -1));

		if (fd.mBakeDirty) {
			// Copy volume
			set<string> kw;
			if (mRawMask) kw.emplace("READ_MASK");
			if (mInvert) kw.emplace("INVERT");
			if (mVolumeColored) kw.emplace("COLORED");
			else if (mColorize) kw.emplace("COLORIZE");
			ComputeShader* copy = mScene->AssetManager()->LoadShader("Shaders/precompute.stm")->GetCompute("CopyRaw", kw);
			vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, copy->mPipeline);
			
			DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("CopyRaw", copy->mDescriptorSetLayouts[0]);
			ds->CreateStorageTextureDescriptor(mRawVolume, copy->mDescriptorBindings.at("RawVolume").second.binding, VK_IMAGE_LAYOUT_GENERAL);
			if (mRawMask) ds->CreateStorageTextureDescriptor(mRawMask, copy->mDescriptorBindings.at("RawMask").second.binding, VK_IMAGE_LAYOUT_GENERAL);
			ds->CreateStorageTextureDescriptor(fd.mBakedVolume, copy->mDescriptorBindings.at("BakedVolume").second.binding, VK_IMAGE_LAYOUT_GENERAL);
			ds->FlushWrites();
			vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, copy->mPipelineLayout, 0, 1, *ds, 0, nullptr);

			commandBuffer->PushConstant(copy, "RemapMin", &mRemapMin);
			commandBuffer->PushConstant(copy, "InvRemapRange", &remapRange);
			commandBuffer->PushConstant(copy, "Cutoff", &mCutoff);
			commandBuffer->PushConstant(copy, "Density", &mDensity);

			vkCmdDispatch(*commandBuffer, (mRawVolume->Width() + 3) / 4, (mRawVolume->Height() + 3) / 4, (mRawVolume->Depth() + 3) / 4);

			fd.mBakedVolume->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);

			fd.mBakeDirty = false;
		}

		// precompute optical density	
		if (fd.mLightingDirty && (mPhysicalShading || mLighting)) {
			set<string> kw;
			ComputeShader* process = mScene->AssetManager()->LoadShader("Shaders/precompute.stm")->GetCompute("ComputeOpticalDensity", kw);
			vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, process->mPipeline);
			
			DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("ComputeOpticalDensity", process->mDescriptorSetLayouts[0]);
			ds->CreateSampledTextureDescriptor(fd.mBakedVolume, process->mDescriptorBindings.at("BakedVolumeS").second.binding, VK_IMAGE_LAYOUT_GENERAL);
			ds->CreateStorageTextureDescriptor(fd.mOpticalDensity, process->mDescriptorBindings.at("BakedOpticalDensity").second.binding, VK_IMAGE_LAYOUT_GENERAL);
			ds->CreateSampledTextureDescriptor(mScene->AssetManager()->LoadTexture("Assets/Textures/rgbanoise.png", false), process->mDescriptorBindings.at("NoiseTex").second.binding);
			ds->FlushWrites();
			float3 vres(fd.mOpticalDensity->Width(), fd.mOpticalDensity->Height(), fd.mOpticalDensity->Depth());
			
			commandBuffer->PushConstant(process, "InvVolumeRotation", &ivr);
			commandBuffer->PushConstant(process, "InvVolumeScale", &ivs);
			commandBuffer->PushConstant(process, "InvViewProj", &ivp);
			commandBuffer->PushConstant(process, "VolumeResolution", &vres);

			commandBuffer->PushConstant(process, "Density", &mDensity);
			commandBuffer->PushConstant(process, "LightDirection", &lightDir);
			commandBuffer->PushConstant(process, "LightColor", &lightCol);

			commandBuffer->PushConstant(process, "StepSize", &mLightStepSize);
			commandBuffer->PushConstant(process, "FrameIndex", &mFrameIndex);

			vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, process->mPipelineLayout, 0, 1, *ds, 0, nullptr);
			vkCmdDispatch(*commandBuffer, (fd.mOpticalDensity->Width() + 3) / 4, (fd.mOpticalDensity->Height() + 3) / 4, (fd.mOpticalDensity->Depth() + 3) / 4);

			fd.mLightingDirty = false;
		}

		float3 vres(fd.mBakedVolume->Width(), fd.mBakedVolume->Height(), fd.mBakedVolume->Depth());

		fd.mBakedVolume->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
		fd.mOpticalDensity->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
		
		#pragma region render volume
		set<string> kw;
		if (mPhysicalShading) kw.emplace("PHYSICAL_SHADING");
		if (mLighting) kw.emplace("LIGHTING");
		if (mColorize) kw.emplace("COLORIZE");
		ComputeShader* draw = mScene->AssetManager()->LoadShader("Shaders/volume.stm")->GetCompute("Draw", kw);
		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, draw->mPipeline);
		
		DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("Draw Volume", draw->mDescriptorSetLayouts[0]);
		ds->CreateSampledTextureDescriptor(fd.mBakedVolume, draw->mDescriptorBindings.at("BakedVolumeS").second.binding, VK_IMAGE_LAYOUT_GENERAL);
		if (mPhysicalShading || mLighting)
			ds->CreateSampledTextureDescriptor(fd.mOpticalDensity, draw->mDescriptorBindings.at("BakedOpticalDensityS").second.binding, VK_IMAGE_LAYOUT_GENERAL);
		ds->CreateStorageTextureDescriptor(camera->ResolveBuffer(0), draw->mDescriptorBindings.at("RenderTarget").second.binding, VK_IMAGE_LAYOUT_GENERAL);
		ds->CreateStorageTextureDescriptor(camera->ResolveBuffer(1), draw->mDescriptorBindings.at("DepthNormal").second.binding, VK_IMAGE_LAYOUT_GENERAL);
		ds->CreateSampledTextureDescriptor(mScene->AssetManager()->LoadTexture("Assets/Textures/rgbanoise.png", false), draw->mDescriptorBindings.at("NoiseTex").second.binding);
		ds->FlushWrites();
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, draw->mPipelineLayout, 0, 1, *ds, 0, nullptr);

		uint2 wo(0);
		float3 vp = mVolumePosition - camera->WorldPosition();

		commandBuffer->PushConstant(draw, "VolumePosition", &vp);
		commandBuffer->PushConstant(draw, "InvVolumeRotation", &ivr);
		commandBuffer->PushConstant(draw, "InvVolumeScale", &ivs);
		commandBuffer->PushConstant(draw, "VolumeResolution", &vres);

		commandBuffer->PushConstant(draw, "LightDirection", &lightDir);
		commandBuffer->PushConstant(draw, "LightColor", &lightCol);
		
		commandBuffer->PushConstant(draw, "Density", &mDensity);
		commandBuffer->PushConstant(draw, "Extinction", &mVolumeExtinction);
		commandBuffer->PushConstant(draw, "Scattering", &mVolumeScatter);
		commandBuffer->PushConstant(draw, "HG", &mVolumePhaseHG);

		commandBuffer->PushConstant(draw, "StepSize", &mStepSize);
		commandBuffer->PushConstant(draw, "FrameIndex", &mFrameIndex);

		switch (camera->StereoMode()) {
		case STEREO_NONE:
			commandBuffer->PushConstant(draw, "InvViewProj", &ivp[0]);
			commandBuffer->PushConstant(draw, "CameraPosition", &cp[0]);
			commandBuffer->PushConstant(draw, "WriteOffset", &wo);
			commandBuffer->PushConstant(draw, "ScreenResolution", &res);
			vkCmdDispatch(*commandBuffer, (camera->FramebufferWidth() + 7) / 8, (camera->FramebufferHeight() + 7) / 8, 1);
			break;
		case STEREO_SBS_HORIZONTAL:
			res.x *= .5f;
			commandBuffer->PushConstant(draw, "InvViewProj", &ivp[0]);
			commandBuffer->PushConstant(draw, "CameraPosition", &cp[0]);
			commandBuffer->PushConstant(draw, "WriteOffset", &wo);
			commandBuffer->PushConstant(draw, "ScreenResolution", &res);
			vkCmdDispatch(*commandBuffer, (camera->FramebufferWidth() / 2 + 7) / 8, (camera->FramebufferHeight() + 7) / 8, 1);
			wo.x = camera->FramebufferWidth() / 2;
			commandBuffer->PushConstant(draw, "InvViewProj", &ivp[1]);
			commandBuffer->PushConstant(draw, "CameraPosition", &cp[1]);
			commandBuffer->PushConstant(draw, "WriteOffset", &wo);
			vkCmdDispatch(*commandBuffer, (camera->FramebufferWidth()/2 + 7) / 8, (camera->FramebufferHeight() + 7) / 8, 1);
			break;
		case STEREO_SBS_VERTICAL:
			res.y *= .5f;
			commandBuffer->PushConstant(draw, "InvViewProj", &ivp[0]);
			commandBuffer->PushConstant(draw, "CameraPosition", &cp[0]);
			commandBuffer->PushConstant(draw, "WriteOffset", &wo);
			commandBuffer->PushConstant(draw, "ScreenResolution", &res);
			vkCmdDispatch(*commandBuffer, (camera->FramebufferWidth() + 7) / 8, (camera->FramebufferHeight() / 2 + 7) / 8, 1);
			wo.y = camera->FramebufferWidth() / 2;
			commandBuffer->PushConstant(draw, "InvViewProj", &ivp[1]);
			commandBuffer->PushConstant(draw, "CameraPosition", &cp[1]);
			commandBuffer->PushConstant(draw, "WriteOffset", &wo);
			vkCmdDispatch(*commandBuffer, (camera->FramebufferWidth() + 7) / 8, (camera->FramebufferHeight() / 2 + 7) / 8, 1);
			break;
		}

		camera->ResolveBuffer(0)->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);

		#pragma endregion

		mFrameIndex++;
	}
	
	Texture* LoadRawStack(const fs::path& folder, Device* device, float3* scale) {
		return nullptr;
	}

	void LoadVolume(CommandBuffer* commandBuffer, const fs::path& folder, bool color) {
		safe_delete(mRawVolume);
		for (uint32_t i = 0; i < commandBuffer->Device()->MaxFramesInFlight(); i++) {
			safe_delete(mFrameData[i].mBakedVolume);
			safe_delete(mFrameData[i].mOpticalDensity);
		}

		Texture* vol;
		if (color) {
			vol = LoadRawStack(folder.string(), mScene->Instance()->Device(), &mVolumeScale);
			if (!vol) {
				fprintf_color(COLOR_RED, stderr, "Failed to load volume!\n");
				return;
			}
		} else {
			vol = Dicom::LoadDicomStack(folder.string(), mScene->Instance()->Device(), &mVolumeScale);
			if (!vol) {
				fprintf_color(COLOR_RED, stderr, "Failed to load volume!\n");
				return;
			}
		}

		mVolumeColored = color;
		
		mVolumeRotation = quaternion(0,0,0,1);
		mVolumePosition = float3(0, 1.6f, 0);
		mRawVolume = vol;
		mRawVolumeNew = true;

		for (uint32_t i = 0; i < commandBuffer->Device()->MaxFramesInFlight(); i++) {
			FrameData& fd = mFrameData[i];
			fd.mBakedVolume = new Texture("Baked Volume", mScene->Instance()->Device(), mRawVolume->Width(), mRawVolume->Height(), mRawVolume->Depth(), VK_FORMAT_R16G16B16A16_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
			fd.mOpticalDensity = new Texture("Baked Optical Density", mScene->Instance()->Device(), mRawVolume->Width()/4, mRawVolume->Height()/4, mRawVolume->Depth()/4, VK_FORMAT_R16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
			fd.mImagesNew = true;
			fd.mBakeDirty = true;
			fd.mLightingDirty = true;
		}
	}
};

ENGINE_PLUGIN(DicomVis)