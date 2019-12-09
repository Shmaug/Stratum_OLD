#include <Core/EnginePlugin.hpp>

#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Scene/TextRenderer.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Util/Profiler.hpp>

#include "TerrainRenderer.hpp"

using namespace std;

class TerrainSystem : public EnginePlugin {
public:
	PLUGIN_EXPORT TerrainSystem();
	PLUGIN_EXPORT ~TerrainSystem();

	PLUGIN_EXPORT bool Init(Scene* scene) override;
	PLUGIN_EXPORT void Update() override;
	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) override;
	PLUGIN_EXPORT void PostRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override;

private:
	Scene* mScene;
	TerrainRenderer* mTerrain;
	vector<Object*> mObjects;
	Object* mSelected;

	float3 mCameraEuler;

	Object* mPlayer;
	Camera* mMainCamera;

	float3 mPlayerVelocity;
	bool mFlying;

	TextRenderer* mFpsText;

	MouseKeyboardInput* mInput;

	float mFrameTimeAccum;
	float mFps;
	uint32_t mFrameCount;
	uint32_t mTriangleCount;
};

ENGINE_PLUGIN(TerrainSystem)

TerrainSystem::TerrainSystem() : mScene(nullptr), mTerrain(nullptr), mSelected(nullptr), mFlying(false) {
	mEnabled = true;
	mCameraEuler = 0;
	mTriangleCount = 0;
	mFrameCount = 0;
	mFrameTimeAccum = 0;
	mPlayerVelocity = 0;
	mFps = 0;
}
TerrainSystem::~TerrainSystem() {
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
}

bool TerrainSystem::Init(Scene* scene) {
	mScene = scene;

	shared_ptr<Material> mat = make_shared<Material>("Terrain", mScene->AssetManager()->LoadShader("Shaders/terrain.shader"));
	shared_ptr<Material> rockMat = make_shared<Material>("Rock", mScene->AssetManager()->LoadShader("Shaders/pbr.shader"));
	shared_ptr<Material> treeMat = make_shared<Material>("Tree", mScene->AssetManager()->LoadShader("Shaders/pbr.shader"));

	mat->SetParameter("MainTextures", 0, mScene->AssetManager()->LoadTexture("Assets/grass/grass1_col.png"));
	mat->SetParameter("NormalTextures", 0, mScene->AssetManager()->LoadTexture("Assets/grass/grass1_nrm.png", false));
	mat->SetParameter("MaskTextures", 0, mScene->AssetManager()->LoadTexture("Assets/grass/grass1_msk.png", false));

	mat->SetParameter("MainTextures", 1, mScene->AssetManager()->LoadTexture("Assets/dirt/ground3_col.jpg"));
	mat->SetParameter("NormalTextures", 1, mScene->AssetManager()->LoadTexture("Assets/dirt/ground3_nrm.jpg", false));
	mat->SetParameter("MaskTextures", 1, mScene->AssetManager()->LoadTexture("Assets/dirt/ground3_msk.jpg", false));

	mat->SetParameter("MainTextures", 2, mScene->AssetManager()->LoadTexture("Assets/rock/rock13_col.jpg"));
	mat->SetParameter("NormalTextures", 2, mScene->AssetManager()->LoadTexture("Assets/rock/rock13_nrm.jpg", false));
	mat->SetParameter("MaskTextures", 2, mScene->AssetManager()->LoadTexture("Assets/rock/rock13_msk.png", false));

	mat->SetParameter("MainTextures", 3, mScene->AssetManager()->LoadTexture("Assets/snow/Snow06_col.jpg"));
	mat->SetParameter("NormalTextures", 3, mScene->AssetManager()->LoadTexture("Assets/snow/Snow06_nrm.jpg", false));
	mat->SetParameter("MaskTextures", 3, mScene->AssetManager()->LoadTexture("Assets/snow/Snow06_msk.png", false));

	Mesh* treeMesh = mScene->AssetManager()->LoadMesh("Assets/tree/oaktree.fbx", .05f);

	treeMat->SetParameter("MainTexture", mScene->AssetManager()->LoadTexture("Assets/tree/bark_col.png"));
	treeMat->SetParameter("NormalTexture", mScene->AssetManager()->LoadTexture("Assets/tree/bark_nrm.png"));
	treeMat->SetParameter("MaskTexture", mScene->AssetManager()->LoadTexture("Assets/tree/bark_msk.png"));
	treeMat->SetParameter("Color", float4(1));
	treeMat->SetParameter("Roughness", 1.f);
	treeMat->SetParameter("Metallic", 1.f);
	treeMat->SetParameter("BumpStrength", 1.f);
	treeMat->EnableKeyword("COLOR_MAP");
	treeMat->EnableKeyword("NORMAL_MAP");
	treeMat->EnableKeyword("MASK_MAP");
	treeMat->PassMask((PassType)(Main | Depth));

	rockMat->SetParameter("MainTexture", mScene->AssetManager()->LoadTexture("Assets/rock_mesh/rock_col.png"));
	rockMat->SetParameter("NormalTexture", mScene->AssetManager()->LoadTexture("Assets/rock_mesh/rock_nrm.png"));
	rockMat->SetParameter("MaskTexture", mScene->AssetManager()->LoadTexture("Assets/rock_mesh/rock_msk.png"));
	rockMat->SetParameter("Color", float4(1));
	rockMat->SetParameter("Roughness", 1.f);
	rockMat->SetParameter("Metallic", 1.f);
	rockMat->SetParameter("BumpStrength", 1.f);
	rockMat->EnableKeyword("COLOR_MAP");
	rockMat->EnableKeyword("NORMAL_MAP");
	rockMat->EnableKeyword("MASK_MAP");
	rockMat->PassMask((PassType)(Main | Depth));

	shared_ptr<TerrainRenderer> terrain = make_shared<TerrainRenderer>("Terrain", 8192.f, 800.f);
	mScene->AddObject(terrain);
	terrain->Material(mat);
	mTerrain = terrain.get();
	mObjects.push_back(mTerrain);
	mTerrain->Initialize();

	shared_ptr<Object> player = make_shared<Object>("Player");
	mScene->AddObject(player);
	mPlayer = player.get();
	mObjects.push_back(mPlayer);
	mPlayer->LocalPosition(0, mTerrain->Height(0), 0);

	shared_ptr<MeshRenderer> rock = make_shared<MeshRenderer>("Rock");
	rock->Mesh(mScene->AssetManager()->LoadMesh("Assets/rock_mesh/rock.fbx", .2f));
	rock->Material(rockMat);
	rock->LocalPosition(0, mTerrain->Height(float3(10, 0, -10)) - .25f, -10);
	mScene->AddObject(rock);
	mObjects.push_back(rock.get());

	shared_ptr<MeshRenderer> tree = make_shared<MeshRenderer>("Tree");
	tree->Mesh(treeMesh);
	tree->Material(treeMat);
	tree->LocalPosition(10, mTerrain->Height(float3(10, 0, 10)), 10);
	mScene->AddObject(tree);
	mObjects.push_back(tree.get());

	shared_ptr<Camera> camera = make_shared<Camera>("Camera", mScene->Instance()->GetWindow(0));
	mScene->AddObject(camera);
	camera->Near(.01f);
	camera->Far(8192.f);
	camera->FieldOfView(radians(65.f));
	camera->LocalPosition(0, 1.6f, 0);
	mMainCamera = camera.get();
	mPlayer->AddChild(mMainCamera);
	mObjects.push_back(mMainCamera);

	shared_ptr<TextRenderer> fpsText = make_shared<TextRenderer>("Fps Text");
	mScene->AddObject(fpsText);
	fpsText->Font(mScene->AssetManager()->LoadFont("Assets/OpenSans-Regular.ttf", 36));
	fpsText->Text("");
	fpsText->VerticalAnchor(Maximum);
	fpsText->HorizontalAnchor(Minimum);
	mFpsText = fpsText.get();
	mObjects.push_back(mFpsText);

	shared_ptr<Light> light = make_shared<Light>("Light");
	mScene->AddObject(light);
	mObjects.push_back(light.get());
	light->LocalPosition(0, mTerrain->Height(0) + 5, 0);
	light->LocalRotation(quaternion(float3(PI * .4f)));
	light->Type(Spot);
	light->InnerSpotAngle(radians(20.f));
	light->OuterSpotAngle(radians(25.f));
	light->CastShadows(true);
	light->Range(100);
	light->Color(float3(1, .9f, .9f));
	light->Intensity(100);

	mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	return true;
}

void TerrainSystem::Update() {
	float tod = mScene->Environment()->TimeOfDay();
	tod += mScene->Instance()->DeltaTime() * .000555555555f; // 30min days
	if (mInput->KeyDown(GLFW_KEY_T)) {
		tod += mScene->Instance()->DeltaTime() * .075f; // zoooom
		if (tod > 1) tod -= 1;
	}
	mScene->Environment()->TimeOfDay(tod);

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
	if (mInput->KeyDown(GLFW_KEY_W)) move.z += 1;
	if (mInput->KeyDown(GLFW_KEY_S)) move.z -= 1;
	if (mInput->KeyDown(GLFW_KEY_D)) move.x += 1;
	if (mInput->KeyDown(GLFW_KEY_A)) move.x -= 1;
	move = (mFlying ? mMainCamera->WorldRotation() : mPlayer->WorldRotation()) * move;
	if (dot(move, move) > .001f) {
		move = normalize(move);
		move *= 2.5f;
		if (mInput->KeyDown(GLFW_KEY_LEFT_SHIFT))
			move *= 2.5f;
	}
	if (mFlying) move *= 100.f;
	float3 mf = 5 * (move - mPlayerVelocity) * mScene->Instance()->DeltaTime();
	if (!mFlying) mf.y = 0;
	mPlayerVelocity += mf;
	#pragma endregion

	if (!mFlying) mPlayerVelocity.y -= 9.8f * mScene->Instance()->DeltaTime(); // gravity

	float3 p = mPlayer->WorldPosition();
	p += mPlayerVelocity * mScene->Instance()->DeltaTime(); // integrate velocity

	float ty = mTerrain->Height(p) + 1.6f;
	if (p.y < ty) {
		p.y = ty;
		mPlayerVelocity.y = 0;

		if (!mFlying && mInput->KeyDown(GLFW_KEY_SPACE))
			mPlayerVelocity.y += 4.f;

	}
	mPlayer->LocalPosition(p);
	#pragma endregion

	if (mInput->MouseButtonDownFirst(GLFW_MOUSE_BUTTON_RIGHT))
		mInput->LockMouse(!mInput->LockMouse());
	if (mInput->KeyDownFirst(GLFW_KEY_F1))
		mScene->DrawGizmos(!mScene->DrawGizmos());
	if (mInput->KeyDownFirst(GLFW_KEY_F2))
		mFlying = !mFlying;

	if (mInput->KeyDownFirst(GLFW_KEY_F9))
		mMainCamera->Orthographic(!mMainCamera->Orthographic());

	mTerrain->UpdateLOD(mMainCamera);


	if (mMainCamera->Orthographic()) {
		mFpsText->TextScale(.028f * mMainCamera->OrthographicSize());
		mMainCamera->OrthographicSize(mMainCamera->OrthographicSize() * (1 - mInput->ScrollDelta().y * .06f));
	} else
		mFpsText->TextScale(.0005f * tanf(mMainCamera->FieldOfView() / 2));
	mFpsText->LocalRotation(mMainCamera->WorldRotation());
	mFpsText->LocalPosition(mMainCamera->ClipToWorld(float3(-.99f, -.96f, 0.005f)));

	// count fps
	mFrameTimeAccum += mScene->Instance()->DeltaTime();
	mFrameCount++;
	if (mFrameTimeAccum > 1.f) {
		mFps = mFrameCount / mFrameTimeAccum;
		mFrameTimeAccum -= 1.f;
		mFrameCount = 0;
		char buf[256];
		sprintf(buf, "%.2f fps | %d tris\n", mFps, mTriangleCount);
		mFpsText->Text(buf);
	}
}

void TerrainSystem::PostRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	mTriangleCount = commandBuffer->mTriangleCount;
}

void TerrainSystem::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	const Ray& ray = mInput->GetPointer(0)->mWorldRay;
	float hitT;
	Collider* hit = mScene->Raycast(ray, hitT);

	Gizmos* gizmos = mScene->Gizmos();

	bool change = mInput->MouseButtonDownFirst(GLFW_MOUSE_BUTTON_LEFT);

	// manipulate selection
	Light* selectedLight = nullptr;
	if (mSelected) {
		selectedLight = dynamic_cast<Light*>(mSelected);
		if (selectedLight) {
			switch (selectedLight->Type()) {
			case LightType::Spot:
				gizmos->DrawWireSphere(selectedLight->WorldPosition(), selectedLight->Radius(), float4(selectedLight->Color(), .5f));
				gizmos->DrawWireCircle(selectedLight->WorldPosition() + selectedLight->WorldRotation() * float3(0, 0, selectedLight->Range()),
					selectedLight->Range() * tanf(selectedLight->InnerSpotAngle()), selectedLight->WorldRotation(), float4(selectedLight->Color(), .5f));
				gizmos->DrawWireCircle(
					selectedLight->WorldPosition() + selectedLight->WorldRotation() * float3(0, 0, selectedLight->Range()),
					selectedLight->Range() * tanf(selectedLight->OuterSpotAngle()), selectedLight->WorldRotation(), float4(selectedLight->Color(), .5f));
				break;

			case LightType::Point:
				gizmos->DrawWireSphere(selectedLight->WorldPosition(), selectedLight->Radius(), float4(selectedLight->Color(), .5f));
				gizmos->DrawWireSphere(selectedLight->WorldPosition(), selectedLight->Range(), float4(selectedLight->Color(), .2f));
				break;
			}
		}

		float s = camera->Orthographic() ? .05f : .05f * length(mSelected->WorldPosition() - camera->WorldPosition());
		if (mInput->KeyDown(GLFW_KEY_LEFT_SHIFT)) {
			quaternion r = mSelected->WorldRotation();
			if (mScene->Gizmos()->RotationHandle(mInput->GetPointer(0), mSelected->WorldPosition(), r, s)) {
				mSelected->LocalRotation(r);
				change = false;
			}
		} else {
			float3 p = mSelected->WorldPosition();
			if (mScene->Gizmos()->PositionHandle(mInput->GetPointer(0), camera->WorldRotation(), p, s)) {
				mSelected->LocalPosition(p);
				change = false;
			}
		}
	}

	// change selection
	if (change) mSelected = nullptr;
	for (Light* light : mScene->ActiveLights()) {
		float lt = ray.Intersect(Sphere(light->WorldPosition(), .09f)).x;
		bool hover = lt > 0 && (hitT < 0 || lt < hitT);

		float3 col = light->mEnabled ? light->Color() : light->Color() * .2f;
		gizmos->DrawBillboard(light->WorldPosition(), hover && light != selectedLight ? .09f : .075f, camera->WorldRotation(), float4(col, 1),
			mScene->AssetManager()->LoadTexture("Assets/icons.png"), float4(.5f, .5f, 0, 0));

		if (hover) {
			hitT = lt;
			if (change) mSelected = light;
		}
	}
}