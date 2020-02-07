#include <Scene/MeshRenderer.hpp>
#include <Content/Font.hpp>
#include <Scene/GUI.hpp>
#include <Util/Profiler.hpp>

#include <Core/EnginePlugin.hpp>
#include <assimp/pbrmaterial.h>

using namespace std;

class DicomVis : public EnginePlugin {
private:
	Scene* mScene;
	vector<Object*> mObjects;
	Object* mSelected;

	Camera* mMainCamera;

	MouseKeyboardInput* mInput;

	bool mShowPerformance;
	bool mSnapshotPerformance;
	ProfilerSample mProfilerFrames[PROFILER_FRAME_COUNT - 1];
	uint32_t mSelectedFrame;

	float mFrameTimeAccum;
	float mFps;
	uint32_t mFrameCount;

	float mFolderScrollAmount;

	std::set<fs::path> mDicomFolders;

public:
	PLUGIN_EXPORT DicomVis(): mScene(nullptr), mSelected(nullptr), mShowPerformance(false), mSnapshotPerformance(false),
		mFrameCount(0), mFrameTimeAccum(0), mFps(0), mFolderScrollAmount(0) {
		mEnabled = true;
	}
	PLUGIN_EXPORT ~DicomVis() {
		for (Object* obj : mObjects)
			mScene->RemoveObject(obj);
	}

	PLUGIN_EXPORT bool Init(Scene* scene) override {
		mScene = scene;
		mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

		shared_ptr<Camera> camera = make_shared<Camera>("Camera", mScene->Instance()->Window());
		mScene->AddObject(camera);
		camera->Near(.01f);
		camera->Far(800.f);
		camera->FieldOfView(radians(65.f));
		camera->LocalPosition(0, 1.6f, 0);
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



		for (const auto& p : fs::recursive_directory_iterator("/home/tjhedstr/data/dicoms"))
			if (p.path().extension().string() == ".dcm")
				mDicomFolders.emplace(p.path().parent_path());

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
		Font* sem11 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 11);
		Font* reg14 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Regular.ttf", 14);
		Font* sem16 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 16);
		Font* bld24 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Bold.ttf", 24);

		float2 s(camera->FramebufferWidth(), camera->FramebufferHeight());
		float2 c = mInput->CursorPos();
		c.y = s.y - c.y;
		
		if (mShowPerformance) {
			char tmpText[64];

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

			GUI::Rect(commandBuffer, camera, float2(0, 0), float2(s.x, graphHeight), float4(.1f, .1f, .1f, 1));
			GUI::Rect(commandBuffer, camera, float2(0, graphHeight - 1), float2(s.x, 2), float4(.2f, .2f, .2f, 1));

			snprintf(tmpText, 64, "%.1fms", m);
			sem11->DrawString(commandBuffer, camera, tmpText, float4(.6f, .6f, .6f, 1.f), float2(2, graphHeight - 10), 11.f);

			for (float i = 1; i < 3; i++) {
				float x = m * i / 3.f;
				snprintf(tmpText, 32, "%.1fms", x);
				GUI::Rect(commandBuffer, camera, float2(0, graphHeight * (i / 3.f) - 1), float2(s.x, 1), float4(.2f, .2f, .2f, 1));
				sem11->DrawString(commandBuffer, camera, tmpText, float4(.6f, .6f, .6f, 1.f), float2(2, graphHeight * (i / 3.f) + 2), 11.f);
			}

			GUI::DrawScreenLine(commandBuffer, camera, points, pointCount, 0, float2(s.x, graphHeight), float4(.2f, 1.f, .2f, 1.f));

			if (mSnapshotPerformance) {
				if (c.y < 100) {
					uint32_t hvr = (uint32_t)((c.x / s.x) * (PROFILER_FRAME_COUNT - 2) + .5f);
					GUI::Rect(commandBuffer, camera, float2(s.x * hvr / (PROFILER_FRAME_COUNT - 2), 0), float2(1, graphHeight), float4(1, 1, 1, .15f));
					if (mInput->KeyDown(MOUSE_LEFT))
						mSelectedFrame = hvr;
				}

				if (mSelectedFrame < PROFILER_FRAME_COUNT - 1) {
					ProfilerSample* selected = nullptr;
					float sampleHeight = 20;

					// selection line
					GUI::Rect(commandBuffer, camera, float2(s.x * mSelectedFrame / (PROFILER_FRAME_COUNT - 2), 0), float2(1, graphHeight), 1);

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

						GUI::Rect(commandBuffer, camera, pos, size, col);
						GUI::Rect(commandBuffer, camera, pos + 1, size - 2, float4(.3f, .9f, .3f, 1));

						for (auto it = p.first->mChildren.begin(); it != p.first->mChildren.end(); it++)
							samples.push(make_pair(&*it, p.second + 1));
					}

					if (selected) {
						snprintf(tmpText, 64, "%s: %.2fms\n", selected->mLabel, selected->mDuration.count() * 1e-6f);
						GUI::Rect(commandBuffer, camera, float2(0, graphHeight), float2(s.x, 20), float4(0,0,0,.8f));
						reg14->DrawString(commandBuffer, camera, tmpText, 1, float2(s.x * .5f, graphHeight + 8), 14.f, TEXT_ANCHOR_MID, TEXT_ANCHOR_MID);
					}
				}

			}
			#endif

			snprintf(tmpText, 64, "%.2f fps | %llu tris\n", mFps, commandBuffer->mTriangleCount);
			sem16->DrawString(commandBuffer, camera, tmpText, 1.f, float2(5, camera->FramebufferHeight() - 18), 18.f);
		}

		float2 panelSize(300, 400);
		float2 panelPos(10, s.y * .5f - panelSize.y * .5f);

		float2 scrollViewSize(panelSize.x - 10, panelSize.y - 70);
		float2 scrollViewPos(panelPos.x+5, panelPos.y+35);

		GUI::Rect(commandBuffer, camera, panelPos-2, panelSize+4, float4(.3f, .3f, .3f, 1)); // outline
		GUI::Rect(commandBuffer, camera, panelPos, panelSize, float4(.2f, .2f, .2f, 1)); // background
		GUI::Label(commandBuffer, camera, bld24, "Load DICOM", 24, panelPos+float2(0, panelSize.y-35), float2(panelSize.x, 30), 0, 1);
		GUI::Rect(commandBuffer, camera, panelPos+float2(0,panelSize.y-30), float2(panelSize.x, 1), 1); // separator

		if (c.x > scrollViewPos.x && c.y > scrollViewPos.y && c.x < scrollViewPos.x + scrollViewSize.x && c.y < scrollViewPos.y + scrollViewSize.y){
			mFolderScrollAmount -= mInput->ScrollDelta() * 60;
		}
		float scrollMax = max(0, mDicomFolders.size()*20 - scrollViewSize.y);
		mFolderScrollAmount = clamp(mFolderScrollAmount, 0.f, scrollMax);

		float y = scrollViewPos.y + scrollViewSize.y + mFolderScrollAmount - 24;
		for (const fs::path& p : mDicomFolders){
			if (GUI::Button(commandBuffer, camera, sem16, p.stem().string(), 16, scrollViewPos+float2(0, y), float2(scrollViewSize.x, 20), float4(.1f, .1f, .1f, 1), 1, TEXT_ANCHOR_MIN, TEXT_ANCHOR_MID, float4(scrollViewPos, scrollViewSize)))
				LoadVolume(p);
			y -= 20;
		}
		if (scrollMax > 0) {
			// scroll bar
			GUI::Rect(commandBuffer, camera, scrollViewPos + float2(scrollViewSize.x - 6, 0), float2(6, scrollViewSize.y), float4(.4f,.4f,.4f,1));
			GUI::Rect(commandBuffer, camera, scrollViewPos + float2(scrollViewSize.x - 6, (scrollViewSize.y-20) * (1-mFolderScrollAmount/scrollMax)), float2(6, 20), float4(.7f,.7f,.7f,1));
		}
	}

	void LoadVolume(const fs::path& folder){

	}
};

ENGINE_PLUGIN(DicomVis)