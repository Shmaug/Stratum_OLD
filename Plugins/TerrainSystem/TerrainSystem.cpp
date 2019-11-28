#include <Core/EnginePlugin.hpp>

#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Scene/TextRenderer.hpp>
#include <Util/Profiler.hpp>

#include "TerrainRenderer.hpp"

using namespace std;

class TerrainSystem : public EnginePlugin {
public:
	PLUGIN_EXPORT TerrainSystem();
	PLUGIN_EXPORT ~TerrainSystem();

	PLUGIN_EXPORT bool Init(Scene* scene) override;
	PLUGIN_EXPORT void Update() override;
	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera);
	PLUGIN_EXPORT void PostRender(CommandBuffer* commandBuffer, Camera* camera);

private:
	Scene* mScene;
	TerrainRenderer* mTerrain;
	vector<Object*> mObjects;

	float3 mCameraEuler;

	Object* mPlayer;
	Camera* mMainCamera;

	float3 mPlayerVelocity;
	float mHeadBob;

	TextRenderer* mFpsText;

	MouseKeyboardInput* mInput;

	float mFrameTimeAccum;
	float mFps;
	uint32_t mFrameCount;
	uint32_t mTriangleCount;
};

ENGINE_PLUGIN(TerrainSystem)

TerrainSystem::TerrainSystem() : mScene(nullptr), mTerrain(nullptr) {
	mEnabled = true;
	mHeadBob = 0;
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

	shared_ptr<TerrainRenderer> terrain = make_shared<TerrainRenderer>("Terrain", 10000.f, 1000.f);
	mScene->AddObject(terrain);
	terrain->Material(mat);
	mTerrain = terrain.get();
	mObjects.push_back(mTerrain);


	shared_ptr<Light> sun = make_shared<Light>("Sun");
	mScene->AddObject(sun);
	sun->CastShadows(true);
	sun->ShadowDistance(1024);
	sun->Color(float3(1, .99f, .95f));
	sun->LocalRotation(quaternion(float3(PI / 4, PI / 4, 0)));
	sun->Type(Sun);
	mObjects.push_back(sun.get());

	shared_ptr<Object> player = make_shared<Object>("Player");
	mScene->AddObject(player);
	mPlayer = player.get();
	mObjects.push_back(mPlayer);
	mPlayer->LocalPosition(0, mTerrain->Height(0), 0);

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
	camera->AddChild(mFpsText);
	mObjects.push_back(mFpsText);

	mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	return true;
}

void TerrainSystem::Update() {
	if (mInput->KeyDownFirst(GLFW_KEY_F1))
		mScene->DrawGizmos(!mScene->DrawGizmos());

	float3 lp = (mMainCamera->WorldToObject() * float4(mMainCamera->ClipToWorld(float3(-.99f, -.96f, 0)), 1)).xyz;
	lp.z = mMainCamera->Near() + .00001f;
	mFpsText->LocalPosition(lp);

	if (mMainCamera->Orthographic()) {
		mFpsText->TextScale(.028f * mMainCamera->OrthographicSize());
		mMainCamera->OrthographicSize(mMainCamera->OrthographicSize() * (1 - mInput->ScrollDelta().y * .06f));
	} else
		mFpsText->TextScale(.0005f * tanf(mMainCamera->FieldOfView() / 2));

	if (mInput->KeyDownFirst(GLFW_KEY_O))
		mMainCamera->Orthographic(!mMainCamera->Orthographic());


	if (mInput->MouseButtonDownFirst(GLFW_MOUSE_BUTTON_RIGHT))
		mInput->LockMouse(!mInput->LockMouse());

	mMainCamera->FieldOfView(clamp(mMainCamera->FieldOfView() + mInput->ScrollDelta().y * .01f, radians(1.f), radians(110.f)));

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
	move = mPlayer->WorldRotation() * move;
	if (dot(move, move) > .001f) {
		move = normalize(move);
		move *= 2.5f;
		if (mInput->KeyDown(GLFW_KEY_LEFT_SHIFT))
			move *= 2.5f;
	}
	float3 mf = 10.f * (move - mPlayerVelocity) * mScene->Instance()->DeltaTime();
	mf.y = 0;
	mPlayerVelocity += mf;
	#pragma endregion

	mPlayerVelocity.y -= 9.8f * mScene->Instance()->DeltaTime(); // gravity

	float3 p = mPlayer->WorldPosition();
	p += mPlayerVelocity * mScene->Instance()->DeltaTime(); // integrate velocity

	float ty = mTerrain->Height(p) + 1.6f;
	if (p.y < ty) {
		p.y = ty;
		mPlayerVelocity.y = 0;

		if (mInput->KeyDown(GLFW_KEY_SPACE))
			mPlayerVelocity.y += 4.f;

	}
	mPlayer->LocalPosition(p);

	float m = length(move);
	if (p.y - ty < .05f && m > 0)
		mHeadBob += m * mScene->Instance()->DeltaTime();
	else
		mHeadBob = 0;

	float sb = sin(-2 * mHeadBob);
	sb *= sb;
	float dy = 1.6f + .2f * -sb - mMainCamera->LocalPosition().y;
	mMainCamera->LocalPosition(0, mMainCamera->LocalPosition().y + min(mScene->Instance()->DeltaTime(), dy), 0);

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

void TerrainSystem::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	
}

void TerrainSystem::PostRender(CommandBuffer* commandBuffer, Camera* camera) {
	mTriangleCount = commandBuffer->mTriangleCount;
}