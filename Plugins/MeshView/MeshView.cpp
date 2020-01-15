#include <Scene/MeshRenderer.hpp>
#include <Scene/TextRenderer.hpp>
#include <Util/Profiler.hpp>
#include <Util/Tokenizer.hpp>

#include <Core/EnginePlugin.hpp>
#include <assimp/pbrmaterial.h>

#define STB_IMAGE_IMPLEMENTATION
#include <ThirdParty/stb_image.h>

using namespace std;

class MeshView : public EnginePlugin {
public:
	PLUGIN_EXPORT MeshView();
	PLUGIN_EXPORT ~MeshView();

	PLUGIN_EXPORT bool Init(Scene* scene) override;
	PLUGIN_EXPORT void Update() override;
	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera);

private:
	Scene* mScene;
	vector<Object*> mObjects;
	Object* mSelected;

	Object* LoadSkel(const fs::path& file);
	Object* Load(const fs::path& file);

	MouseKeyboardInput* mInput;
};

ENGINE_PLUGIN(MeshView)

MeshView::MeshView() : mScene(nullptr), mSelected(nullptr), mInput(nullptr) {
	mEnabled = true;
}
MeshView::~MeshView() {
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
}

struct Joint {
	string mName;
	float3 mOffset;
	float3 mMin;
	float3 mMax;
	float3 mRotMin;
	float3 mRotMax;
	float3 mPose;
	Joint* mParent;
	vector<Joint*> mChildren;
	inline Joint(const string& name) : mName(name), mOffset(0), mMin(-.1f), mMax(.1f), mRotMin(-1e10f), mRotMax(1e10f), mPose(0), mParent(nullptr), mChildren({}) {};
};

#define THROW_INVALID_SKEL { fprintf_color(COLOR_RED, stderr, "Invalid SKEL file\n"); throw; }

Joint* ReadJoint(Tokenizer& t, const string &name) {
	Joint* j = new Joint(name);

	string token;
	if (!t.Next(token) || token != "{") THROW_INVALID_SKEL

	while (t.Next(token)) {
		if (token == "offset") {
			if (!t.Next(j->mOffset.x)) THROW_INVALID_SKEL
			if (!t.Next(j->mOffset.y)) THROW_INVALID_SKEL
			if (!t.Next(j->mOffset.z)) THROW_INVALID_SKEL
			continue;
		}
		if (token == "boxmin") {
			if (!t.Next(j->mMin.x)) THROW_INVALID_SKEL
			if (!t.Next(j->mMin.y)) THROW_INVALID_SKEL
			if (!t.Next(j->mMin.z)) THROW_INVALID_SKEL
			continue;
		}
		if (token == "boxmax") {
			if (!t.Next(j->mMax.x)) THROW_INVALID_SKEL
			if (!t.Next(j->mMax.y)) THROW_INVALID_SKEL
			if (!t.Next(j->mMax.z)) THROW_INVALID_SKEL
			continue;
		}
		if (token == "rotxlimit") {
			if (!t.Next(j->mRotMin.x)) THROW_INVALID_SKEL
			if (!t.Next(j->mRotMax.x)) THROW_INVALID_SKEL
			continue;
		}
		if (token == "rotylimit") {
			if (!t.Next(j->mRotMin.y)) THROW_INVALID_SKEL
			if (!t.Next(j->mRotMax.y)) THROW_INVALID_SKEL
			continue;
		}
		if (token == "rotzlimit") {
			if (!t.Next(j->mRotMin.z)) THROW_INVALID_SKEL
			if (!t.Next(j->mRotMax.z)) THROW_INVALID_SKEL
			continue;
		}
		if (token == "pose") {
			if (!t.Next(j->mPose.x)) THROW_INVALID_SKEL
			if (!t.Next(j->mPose.y)) THROW_INVALID_SKEL
			if (!t.Next(j->mPose.z)) THROW_INVALID_SKEL
			continue;
		}
		if (token == "balljoint") {
			if (!t.Next(token)) THROW_INVALID_SKEL
			Joint* c = ReadJoint(t, token);
			c->mParent = j;
			j->mChildren.push_back(c);
			continue;
		}

		if (token == "}") break;
	}

	return j;
}

Object* MeshView::LoadSkel(const fs::path& filepath) {
	ifstream file(filepath.string());
	
	Tokenizer t(file, { ' ', '\n', '\r', '\t' });

	string token;
	if (!t.Next(token) || token != "balljoint") THROW_INVALID_SKEL
	if (!t.Next(token)) THROW_INVALID_SKEL
	Joint* rootJoint = ReadJoint(t, token);

	if (t.Next(token)) {
		if (token == "balljoint"){
			fprintf_color(COLOR_RED, stderr, "Multiple root joints!\n");
			throw;
		}
		THROW_INVALID_SKEL
	}

	uint32_t jc = 0;
	queue<Joint*> joints;
	joints.push(rootJoint);
	while (!joints.empty()){
		jc++;
		Joint* j = joints.front();
		joints.pop();
		for (Joint* c : j->mChildren) joints.push(c);
	}

	printf("Loaded %s (%u joints)\n", filepath.string().c_str(), jc);

	return nullptr;
}

Object* MeshView::Load(const fs::path& filepath) {
	string ext = filepath.has_extension() ? filepath.extension().string() : "";
	if (ext == ".skel")
		return LoadSkel(filepath);
	// load with Assimp
	return nullptr;
}

bool MeshView::Init(Scene* scene) {
	mScene = scene;
	mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	mScene->Environment()->EnableCelestials(true);
	mScene->Environment()->EnableScattering(true);

	shared_ptr<Material> pbrMat = make_shared<Material>("Floor", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	pbrMat->EnableKeyword("TEXTURED");
	pbrMat->SetParameter("MainTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/grid.png"));
	pbrMat->SetParameter("NormalTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/bump.png"));
	pbrMat->SetParameter("MaskTextures", 0, mScene->AssetManager()->LoadTexture("Assets/Textures/mask.png"));
	pbrMat->SetParameter("TextureST", float4(2048, 2048, 0, 0));
	pbrMat->SetParameter("TextureIndex", 0);
	pbrMat->SetParameter("Color", float4(1, 1, 1, 1));
	pbrMat->SetParameter("Metallic", 0.f);
	pbrMat->SetParameter("Roughness", .5f);

	shared_ptr<MeshRenderer> floor = make_shared<MeshRenderer>("Floor");
	floor->Material(pbrMat);
	floor->Mesh(shared_ptr<Mesh>(Mesh::CreatePlane("Floor", mScene->Instance(), 2048)));
	floor->LocalRotation(quaternion(float3(-PI*.5f, 0, 0)));
	mScene->AddObject(floor);
	mObjects.push_back(floor.get());

	Load("Assets/Models/dragon.skel");
	Load("Assets/Models/wasp.skel");

	return true;
}

void MeshView::Update() {
	float tod = mScene->Environment()->TimeOfDay();
	tod += mScene->Instance()->DeltaTime() * .000555555555f; // 30min days
	if (mInput->KeyDown(KEY_T)) {
		tod += mScene->Instance()->DeltaTime() * .075f; // zoooom
		if (tod > 1) tod -= 1;
	}
	mScene->Environment()->TimeOfDay(tod);
}
void MeshView::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	const Ray& ray = mInput->GetPointer(0)->mWorldRay;
	float hitT;
	Collider* hit = mScene->Raycast(ray, hitT);

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