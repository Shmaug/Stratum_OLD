#include <Scene/MeshRenderer.hpp>
#include <Scene/TextRenderer.hpp>
#include <Util/Profiler.hpp>

#include <Core/EnginePlugin.hpp>
#include <assimp/pbrmaterial.h>

using namespace std;

class DicomVis : public EnginePlugin {
public:
	PLUGIN_EXPORT DicomVis();
	PLUGIN_EXPORT ~DicomVis();

	PLUGIN_EXPORT bool Init(Scene* scene) override;
	PLUGIN_EXPORT void Update() override;

	PLUGIN_EXPORT void PostRenderScene(CommandBuffer* commandBuffer, Camera* camera, PassType pass);
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

	MouseKeyboardInput* mInput;

	float mFrameTimeAccum;
	float mFps;
	uint32_t mFrameCount;

	bool mPrintPerformance;
};

ENGINE_PLUGIN(DicomVis)

DicomVis::DicomVis()
	: mScene(nullptr), mSelected(nullptr), mFlying(false), mPrintPerformance(false), mCameraEuler(0), mFrameCount(0), mFrameTimeAccum(0), mFps(0), mPlayerVelocity(0) {
	mEnabled = true;
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
		mObjects.push_back(o);
		for (uint32_t i = 0; i < o->ChildCount(); i++)
			nodes.push(o->Child(i));
	}
	#pragma endregion

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
	if (mInput->KeyDownFirst(KEY_F3))
		mPrintPerformance = !mPrintPerformance;

	// count fps
	mFrameTimeAccum += mScene->Instance()->DeltaTime();
	mFrameCount++;
	if (mFrameTimeAccum > 1.f) {
		mFps = mFrameCount / mFrameTimeAccum;
		mFrameTimeAccum -= 1.f;
		mFrameCount = 0;
	}
}

void DicomVis::PostRenderScene(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
		if (pass != PASS_MAIN || camera != mScene->Cameras()[0]) return;

	Font* reg = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Regular.ttf", 18);
	Font* bld = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Bold.ttf", 16);

	if (mPrintPerformance) {
		#ifdef PROFILER_ENABLE
		#endif

		char perfText[64];
		snprintf(perfText, 64, "%.2f fps | %llu tris\n", mFps, commandBuffer->mTriangleCount);
		bld->DrawScreenString(commandBuffer, camera, perfText, 1.f, float2(5, camera->FramebufferHeight() - 18), 18.f);

	}
}

void DicomVis::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	const Ray& ray = mInput->GetPointer(0)->mWorldRay;
	float hitT;
	RaycastReceiver* hit = mScene->Raycast(ray, hitT);

	Gizmos* gizmos = mScene->Gizmos();

	bool change = mInput->KeyDownFirst(MOUSE_LEFT);

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
		if (mInput->KeyDown(KEY_LSHIFT)) {
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