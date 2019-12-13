#include <Scene/MeshRenderer.hpp>
#include <Scene/TextRenderer.hpp>
#include <Util/Profiler.hpp>

#include <Core/EnginePlugin.hpp>

#include <thread>

using namespace std;

class DicomVis : public EnginePlugin {
public:
	PLUGIN_EXPORT DicomVis();
	PLUGIN_EXPORT ~DicomVis();

	PLUGIN_EXPORT bool Init(Scene* scene) override;
	PLUGIN_EXPORT void Update() override;

	PLUGIN_EXPORT void PostRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass);
	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera);

private:
	Scene* mScene;
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
	size_t mTriangleCount;
};

ENGINE_PLUGIN(DicomVis)

DicomVis::DicomVis() : mScene(nullptr), mSelected(nullptr), mFlying(false) {
	mEnabled = true;
	mCameraEuler = 0;
	mTriangleCount = 0;
	mFrameCount = 0;
	mFrameTimeAccum = 0;
	mPlayerVelocity = 0;
	mFps = 0;
}
DicomVis::~DicomVis() {
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
}

bool DicomVis::Init(Scene* scene) {
	mScene = scene;
	mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	shared_ptr<Object> player = make_shared<Object>("Player");
	mScene->AddObject(player);
	mPlayer = player.get();
	mObjects.push_back(mPlayer);

	shared_ptr<Camera> camera = make_shared<Camera>("Camera", mScene->Instance()->GetWindow(0));
	mScene->AddObject(camera);
	camera->Near(.01f);
	camera->Far(800.f);
	camera->FieldOfView(radians(65.f));
	camera->LocalPosition(0, 1.6f, 0);
	mMainCamera = camera.get();
	mPlayer->AddChild(mMainCamera);
	mObjects.push_back(mMainCamera);

	shared_ptr<TextRenderer> fpsText = make_shared<TextRenderer>("Fps Text");
	mScene->AddObject(fpsText);
	fpsText->Font(mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Regular.ttf", 36));
	fpsText->Text("");
	fpsText->VerticalAnchor(Maximum);
	fpsText->HorizontalAnchor(Minimum);
	mFpsText = fpsText.get();
	mObjects.push_back(mFpsText);

	Shader* pbrShader = mScene->AssetManager()->LoadShader("Shaders/pbr.shader");
	auto func = [](Scene* s, aiMaterial* m, void* data) {
		shared_ptr<Material> mat = make_shared<Material>("PBR", (Shader*)data);
		std::string name = m->GetName().C_Str();
		if (name == "MetalBrushed") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(1));
			mat->SetParameter("Roughness", 1.f);
			mat->SetParameter("Metallic", 1.f);
			mat->SetParameter("MainTexture",   s->AssetManager()->LoadTexture("Assets/Textures/metalbrushed/metalbrushed_col.png"));
			mat->SetParameter("NormalTexture", s->AssetManager()->LoadTexture("Assets/Textures/metalbrushed/metalbrushed_nrm.png", false));
			mat->SetParameter("MaskTexture",   s->AssetManager()->LoadTexture("Assets/Textures/metalbrushed/metalbrushed_msk.jpg", false));
			mat->EnableKeyword("COLOR_MAP");
			mat->EnableKeyword("NORMAL_MAP");
			mat->EnableKeyword("MASK_MAP");
		} else if (name == "MetalPainted") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(.8f));
			mat->SetParameter("Roughness", .3f);
			mat->SetParameter("Metallic", 1.f);
		} else if (name == "MetalSmooth") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(.9f));
			mat->SetParameter("Roughness", .05f);
			mat->SetParameter("Metallic", 1.f);
		} else if (name == "DarkMetal") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(.4f));
			mat->SetParameter("Roughness", .6f);
			mat->SetParameter("Metallic", 1.f);
		} else if (name == "WhitePlastic") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(.95));
			mat->SetParameter("Roughness", .3f);
			mat->SetParameter("Metallic", .0f);
		} else if (name == "BlackPlastic") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(.1));
			mat->SetParameter("Roughness", .3f);
			mat->SetParameter("Metallic", .0f);
		} else if (name == "Wood") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(1));
			mat->SetParameter("Roughness", 1.f);
			mat->SetParameter("Metallic", 1.f);
			mat->SetParameter("MainTexture",   s->AssetManager()->LoadTexture("Assets/Textures/wood10/Wood10_col.png"));
			mat->SetParameter("NormalTexture", s->AssetManager()->LoadTexture("Assets/Textures/wood10/Wood10_nrm.png", false));
			mat->SetParameter("MaskTexture",   s->AssetManager()->LoadTexture("Assets/Textures/wood10/Wood10_msk.png", false));
			mat->EnableKeyword("COLOR_MAP");
			mat->EnableKeyword("NORMAL_MAP");
			mat->EnableKeyword("MASK_MAP");
		} else if (name == "Wood2") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(1));
			mat->SetParameter("Roughness", 1.f);
			mat->SetParameter("Metallic", 1.f);
			mat->SetParameter("MainTexture",   s->AssetManager()->LoadTexture("Assets/Textures/wood02/wood02_col.jpg"));
			mat->SetParameter("NormalTexture", s->AssetManager()->LoadTexture("Assets/Textures/wood02/wood02_nrm.jpg", false));
			mat->SetParameter("MaskTexture",   s->AssetManager()->LoadTexture("Assets/Textures/wood02/wood02_msk.jpg", false));
			mat->EnableKeyword("COLOR_MAP");
			mat->EnableKeyword("NORMAL_MAP");
			mat->EnableKeyword("MASK_MAP");
		} else if (name == "Hardwood") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(.3f));
			mat->SetParameter("Roughness", 1.f);
			mat->SetParameter("Metallic", 1.f);
			mat->SetParameter("MainTexture",   s->AssetManager()->LoadTexture("Assets/Textures/wood2/wood2_col.png"));
			mat->SetParameter("NormalTexture", s->AssetManager()->LoadTexture("Assets/Textures/wood2/wood2_nrm.png", false));
			mat->SetParameter("MaskTexture",   s->AssetManager()->LoadTexture("Assets/Textures/wood2/wood2_msk.png", false));
			mat->EnableKeyword("COLOR_MAP");
			mat->EnableKeyword("NORMAL_MAP");
			mat->EnableKeyword("MASK_MAP");
		} else if (name == "Wall") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(1));
			mat->SetParameter("Roughness", 1.f);
			mat->SetParameter("Metallic", 1.f);
			mat->SetParameter("MainTexture",   s->AssetManager()->LoadTexture("Assets/Textures/concrete04/Concrete04_col.jpg"));
			mat->SetParameter("NormalTexture", s->AssetManager()->LoadTexture("Assets/Textures/concrete04/Concrete04_nrm.jpg", false));
			mat->SetParameter("MaskTexture",   s->AssetManager()->LoadTexture("Assets/Textures/concrete04/Concrete04_msk.jpg", false));
			mat->EnableKeyword("COLOR_MAP");
			mat->EnableKeyword("NORMAL_MAP");
			mat->EnableKeyword("MASK_MAP");
		} else if (name == "Paper") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(1));
			mat->SetParameter("Roughness", 1.f);
			mat->SetParameter("Metallic", 1.f);
			mat->SetParameter("MainTexture",   s->AssetManager()->LoadTexture("Assets/Textures/paper01/paper01_col.png"));
			mat->SetParameter("NormalTexture", s->AssetManager()->LoadTexture("Assets/Textures/paper01/paper01_nrm.png", false));
			mat->SetParameter("MaskTexture",   s->AssetManager()->LoadTexture("Assets/Textures/paper01/paper01_msk.jpg", false));
			mat->EnableKeyword("COLOR_MAP");
			mat->EnableKeyword("NORMAL_MAP");
			mat->EnableKeyword("MASK_MAP");
		} else if (name == "Leather") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(1));
			mat->SetParameter("Roughness", 1.f);
			mat->SetParameter("Metallic", 1.f);
			mat->SetParameter("MainTexture",   s->AssetManager()->LoadTexture("Assets/Textures/leather13/Leather13_col.jpg"));
			mat->SetParameter("NormalTexture", s->AssetManager()->LoadTexture("Assets/Textures/leather13/Leather13_nrm.jpg", false));
			mat->SetParameter("MaskTexture",   s->AssetManager()->LoadTexture("Assets/Textures/leather13/Leather13_msk.jpg", false));
			mat->EnableKeyword("COLOR_MAP");
			mat->EnableKeyword("NORMAL_MAP");
			mat->EnableKeyword("MASK_MAP");
		} else if (name == "Palette") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(1));
			mat->SetParameter("Roughness", .2f);
			mat->SetParameter("Metallic", .0f);
			mat->SetParameter("MainTexture", s->AssetManager()->LoadTexture("Assets/Textures/palette.png"));
			mat->EnableKeyword("COLOR_MAP");
		} else if (name == "ClearGlass") {
			mat->PassMask((PassType)(Main));
			mat->RenderQueue(5000);
			mat->SetParameter("Color", float4(.9f, .9f, 1, .25f));
			mat->SetParameter("Roughness", .1f);
			mat->SetParameter("Metallic", 1.f);
			mat->BlendMode(Alpha);
			mat->CullMode(VK_CULL_MODE_NONE);
		} else if (name == "TableGlass") {
			mat->PassMask((PassType)(Main));
			mat->RenderQueue(5000);
			mat->SetParameter("Color", float4(1, 1, 1, .75f));
			mat->SetParameter("Roughness", .6f);
			mat->SetParameter("Metallic", 1.f);
			mat->BlendMode(Alpha);
		} else if (name == "Ceiling") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(.6f));
			mat->SetParameter("Roughness", 1.f);
			mat->SetParameter("Metallic", 1.f);
			mat->SetParameter("MainTexture",   s->AssetManager()->LoadTexture("Assets/Textures/woodplank/woodplank_col.png"));
			mat->SetParameter("NormalTexture", s->AssetManager()->LoadTexture("Assets/Textures/woodplank/woodplank_nrm.png", false));
			mat->SetParameter("MaskTexture",   s->AssetManager()->LoadTexture("Assets/Textures/woodplank/woodplank_msk.png", false));
			mat->EnableKeyword("COLOR_MAP");
			mat->EnableKeyword("NORMAL_MAP");
			mat->EnableKeyword("MASK_MAP");
		} else if (name == "Fabric") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(.6f));
			mat->SetParameter("Roughness", 1.f);
			mat->SetParameter("Metallic", 1.f);
			mat->SetParameter("MainTexture",   s->AssetManager()->LoadTexture("Assets/Textures/fabric01/Fabric01_col.jpg"));
			mat->SetParameter("NormalTexture", s->AssetManager()->LoadTexture("Assets/Textures/fabric01/Fabric01_nrm.jpg", false));
			mat->SetParameter("MaskTexture",   s->AssetManager()->LoadTexture("Assets/Textures/fabric01/Fabric01_msk.jpg", false));
			mat->EnableKeyword("COLOR_MAP");
			mat->EnableKeyword("NORMAL_MAP");
			mat->EnableKeyword("MASK_MAP");
		} else if (name == "Carpet") {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(.405f, .594f, .714f, 1.f));
			mat->SetParameter("Roughness", 1.f);
			mat->SetParameter("Metallic", 1.f);
			mat->SetParameter("MainTexture",   s->AssetManager()->LoadTexture("Assets/Textures/carpet01/Carpet01_col.jpg"));
			mat->SetParameter("NormalTexture", s->AssetManager()->LoadTexture("Assets/Textures/carpet01/Carpet01_nrm.jpg", false));
			mat->SetParameter("MaskTexture",   s->AssetManager()->LoadTexture("Assets/Textures/carpet01/Carpet01_msk.jpg", false));
			mat->EnableKeyword("COLOR_MAP");
			mat->EnableKeyword("NORMAL_MAP");
			mat->EnableKeyword("MASK_MAP");
		} else {
			mat->PassMask((PassType)(Main | Depth | Shadow));
			mat->SetParameter("Color", float4(.9f));
			mat->SetParameter("Roughness", .5f);
			mat->SetParameter("Metallic", 0.f);
			printf("\t\"%s\"\n", name.c_str());
		}
		return mat;
	};
	mScene->LoadModelScene("Assets/Models/room.fbx", func, pbrShader, .01f)->LocalPosition(0, .01f, 0);
	
	return true;
}

void DicomVis::Update() {
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
		sprintf(buf, "%.2f fps | %llu tris\n", mFps, mTriangleCount);
		mFpsText->Text(buf);
	}
}

void DicomVis::PostRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	mTriangleCount = commandBuffer->mTriangleCount;
}

void DicomVis::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {}