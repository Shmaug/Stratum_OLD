#include <Scene/MeshRenderer.hpp>
#include <Scene/TextRenderer.hpp>
#include <Util/Profiler.hpp>

#include <Core/EnginePlugin.hpp>
#include <assimp/pbrmaterial.h>

#define STB_IMAGE_IMPLEMENTATION
#include <ThirdParty/stb_image.h>

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

	// set window icon
	int comp;
	GLFWimage icon;
	icon.pixels = stbi_load("Assets/DicomVis.png", &icon.width, &icon.height, &comp, 4);
	for (uint32_t i = 0; i < mScene->Instance()->WindowCount(); i++) {
		mScene->Instance()->GetWindow(i)->Icon(&icon);
		mScene->Instance()->GetWindow(i)->Title("DicomVis");
	}
	stbi_image_free(icon.pixels);

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

	shared_ptr<Material> transparent = make_shared<Material>("Transparent PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	transparent->RenderQueue(5000);
	transparent->BlendMode(BLEND_MODE_ALPHA);
	transparent->CullMode(VK_CULL_MODE_NONE);
	transparent->EnableKeyword("TEXTURED");

	shared_ptr<Material> material = make_shared<Material>("PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	material->EnableKeyword("TEXTURED");

	uint32_t matidx = 0;
	uint32_t transidx = 0;

	auto matfunc = [&](Scene* scene, aiMaterial* aimaterial) {
		aiString alphaMode;
		if (aimaterial->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS)
			if (alphaMode == aiString("BLEND"))
				return transparent;
		return material;
	};
	auto objfunc = [&](Scene* scene, Object* object, aiMaterial* aimaterial) {
		MeshRenderer* renderer = dynamic_cast<MeshRenderer*>(object);
		if (!renderer) return;
		
		Material* mat = renderer->Material();
		uint32_t idx;

		aiString alphaMode;
		if (mat == transparent.get()) {
			idx = transidx;
			transidx++;
		} else {
			idx = matidx;
			matidx++;
		}

		aiColor3D emissiveColor(0);
		aiColor4D baseColor(1);
		float metallic = 1.f;
		float roughness = 1.f;
		aiString baseColorTexture, metalRoughTexture, normalTexture, emissiveTexture;

		if (aimaterial->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE, &baseColorTexture) == AI_SUCCESS && baseColorTexture.length) {
			mat->SetParameter("MainTextures", idx, scene->AssetManager()->LoadTexture("Assets/Models/room/" + string(baseColorTexture.C_Str())));
			baseColor = aiColor4D(1);
		} else
			mat->SetParameter("MainTextures", idx, scene->AssetManager()->LoadTexture("Assets/Textures/white.png", false));

		if (aimaterial->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalRoughTexture) == AI_SUCCESS && metalRoughTexture.length)
			mat->SetParameter("MaskTextures", idx, scene->AssetManager()->LoadTexture("Assets/Models/room/" + string(metalRoughTexture.C_Str()), false));
		else
			mat->SetParameter("MaskTextures", idx, scene->AssetManager()->LoadTexture("Assets/Textures/white.png", false));

		if (aimaterial->GetTexture(aiTextureType_NORMALS, 0, &normalTexture) == AI_SUCCESS && normalTexture.length)
			mat->SetParameter("NormalTextures", idx, scene->AssetManager()->LoadTexture("Assets/Models/room/" + string(normalTexture.C_Str()), false));
		else
			mat->SetParameter("NormalTextures", idx, scene->AssetManager()->LoadTexture("Assets/Textures/bump.png", false));
		
		if (aimaterial->GetTexture(aiTextureType_EMISSIVE, 0, &emissiveTexture) == AI_SUCCESS && emissiveTexture.length) {
			mat->SetParameter("EmissionTextures", idx, scene->AssetManager()->LoadTexture("Assets/Models/room/" + string(emissiveTexture.C_Str())));
			emissiveColor = aiColor3D(1);
		} else
			mat->SetParameter("EmissionTextures", idx, scene->AssetManager()->LoadTexture("Assets/Textures/white.png"));

		aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, baseColor);
		aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, metallic);
		aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, roughness);
		aimaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);

		renderer->PushConstant("TextureIndex", idx);
		renderer->PushConstant("Color", float4(baseColor.r, baseColor.g, baseColor.b, baseColor.a));
		renderer->PushConstant("Roughness", roughness);
		renderer->PushConstant("Metallic", metallic);
		renderer->PushConstant("Emission", float3(emissiveColor.r, emissiveColor.g, emissiveColor.b));
		material->SetParameter("BumpStrength", 2.f);
	};
	
	Object* room = mScene->LoadModelScene("Assets/Models/room/CrohnsProtoRoom.gltf", matfunc, objfunc, 1.f, 1.f, .05f, .006f);
	queue<Object*> nodes;
	nodes.push(room);
	while (nodes.size()) {
		Object* o = nodes.front();
		nodes.pop();
		mObjects.push_back(o);
		for (uint32_t i = 0; i < o->ChildCount(); i++)
			nodes.push(o->Child(i));
	}

	mScene->Environment()->EnableCelestials(false);
	mScene->Environment()->EnableScattering(false);
	mScene->Environment()->AmbientLight(.2f);
	mScene->Environment()->EnvironmentTexture(mScene->AssetManager()->LoadTexture("Assets/Textures/ocean hdri.jpg"));

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

void DicomVis::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
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
			mScene->AssetManager()->LoadTexture("Assets/Textures/icons.png"), float4(.5f, .5f, 0, 0));

		if (hover) {
			hitT = lt;
			if (change) mSelected = light;
		}
	}
}