#include <Scene/MeshRenderer.hpp>
#include <Scene/TextRenderer.hpp>
#include <Util/Profiler.hpp>
#include <Util/Tokenizer.hpp>

#include <Core/EnginePlugin.hpp>
#include <assimp/pbrmaterial.h>

using namespace std;

class SkeletonJoint : public Object {
public:
	AABB mBox;

	float3 mRotMin;
	float3 mRotMax;

	inline SkeletonJoint(const std::string& name) : Object(name), mRotMin(-1e10f), mRotMax(1e10f) {};

	PLUGIN_EXPORT virtual void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) override {
		MouseKeyboardInput* input = Scene()->InputManager()->GetFirst<MouseKeyboardInput>();

		quaternion r = WorldRotation();
		if (Scene()->Gizmos()->RotationHandle(input->GetPointer(0), WorldPosition(), r, .1f)) {
			quaternion lr = Parent() ? inverse(Parent()->WorldRotation()) * r : r;
			lr.x = -lr.x;
			lr.y = -lr.y;
			lr = quaternion(clamp(lr.toEuler(), mRotMin, mRotMax));
			lr.x = -lr.x;
			lr.y = -lr.y;
			LocalRotation(lr);
		}

		float3 center = (ObjectToWorld() * float4(mBox.mCenter, 1.f)).xyz;
		Scene()->Gizmos()->DrawWireCube(center, mBox.mExtents, WorldRotation(), 1.f);
	}
};

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

	SkeletonJoint* ReadJoint(Tokenizer& t, const string& name, float scale = 1.f);
	SkeletonJoint* LoadSkel(const fs::path& file, float scale = 1.f);
	Object* Load(const fs::path& file, float scale = 1.f);

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

#define THROW_INVALID_SKEL { fprintf_color(COLOR_RED, stderr, "Invalid SKEL file\n"); throw; }

SkeletonJoint* MeshView::ReadJoint(Tokenizer& t, const string &name, float scale) {
	shared_ptr<SkeletonJoint> j = make_shared<SkeletonJoint>(name);
	mScene->AddObject(j);
	mObjects.push_back(j.get());

	string token;
	if (!t.Next(token) || token != "{") THROW_INVALID_SKEL

	float3 mn, mx;

	while (t.Next(token)) {
		if (token == "offset") {
			float3 offset;
			if (!t.Next(offset.x)) THROW_INVALID_SKEL
			if (!t.Next(offset.y)) THROW_INVALID_SKEL
			if (!t.Next(offset.z)) THROW_INVALID_SKEL
			offset *=scale;
			offset.z = -offset.z;
			j->LocalPosition(offset);
			continue;
		}
		if (token == "boxmin") {
			if (!t.Next(mn.x)) THROW_INVALID_SKEL
			if (!t.Next(mn.y)) THROW_INVALID_SKEL
			if (!t.Next(mn.z)) THROW_INVALID_SKEL
			mn *= scale;
			mn.z = -mn.z;
			continue;
		}
		if (token == "boxmax") {
			if (!t.Next(mx.x)) THROW_INVALID_SKEL
			if (!t.Next(mx.y)) THROW_INVALID_SKEL
			if (!t.Next(mx.z)) THROW_INVALID_SKEL
			mx *= scale;
			mx.z = -mx.z;
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
			float3 pose;
			if (!t.Next(pose.x)) THROW_INVALID_SKEL
			if (!t.Next(pose.y)) THROW_INVALID_SKEL
			if (!t.Next(pose.z)) THROW_INVALID_SKEL
			quaternion r(pose);
			r.x = -r.x;
			r.y = -r.y;
			j->LocalRotation(r);

			continue;
		}
		if (token == "balljoint") {
			if (!t.Next(token)) THROW_INVALID_SKEL
			j->AddChild(ReadJoint(t, token, scale));
			continue;
		}

		if (token == "}") break;
	}

	j->mBox = AABB((mx + mn) * .5f, (mx - mn) * .5f);

	return j.get();
}

SkeletonJoint* MeshView::LoadSkel(const fs::path& filepath, float scale) {
	ifstream file(filepath.string());
	
	Tokenizer t(file, { ' ', '\n', '\r', '\t' });

	string token;
	if (!t.Next(token) || token != "balljoint") THROW_INVALID_SKEL
	if (!t.Next(token)) THROW_INVALID_SKEL
	
	SkeletonJoint* root = ReadJoint(t, token, scale);

	if (t.Next(token)) {
		if (token == "balljoint"){
			fprintf_color(COLOR_RED, stderr, "Multiple root joints!\n");
			throw;
		}
		THROW_INVALID_SKEL
	}

	printf("Loaded %s\n", filepath.string().c_str());

	return root;
}

Object* MeshView::Load(const fs::path& filepath, float scale) {
	string ext = filepath.has_extension() ? filepath.extension().string() : "";
	if (ext == ".skel")
		return LoadSkel(filepath, scale);
	// load with Assimp
	return nullptr;
}

bool MeshView::Init(Scene* scene) {
	mScene = scene;
	mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	mScene->Environment()->EnableCelestials(false);
	mScene->Environment()->EnableScattering(false);
	mScene->Environment()->AmbientLight(.1f);

	shared_ptr<Light> sun = make_shared<Light>("Light");
	mScene->AddObject(sun);
	mObjects.push_back(sun.get());
	sun->CastShadows(false);
	sun->Color(1);
	sun->LocalPosition(0, 5.f, 0);
	sun->LocalRotation(quaternion(float3(PI / 4, PI / 4, 0)));
	sun->Type(Sun);

	float s = 10;
	uint32_t i = 0;
	float3 positions[]{
		float3( 0, 0,  0),
		float3( s, 0,  0),
		float3(-s, 0,  0),
		float3( 0, 0,  s),
		float3( 0, 0, -s),
		float3( s, 0, -s),
		float3(-s, 0, -s),
		float3( s, 0,  0),
		float3(-s, 0,  0),
	};
	for (const auto& p : fs::directory_iterator("Assets/Models"))
		if (p.path().extension().string() == ".skel")
			Load(p.path().string(), 1)->LocalPosition(positions[i++]);

	return true;
}

void MeshView::Update() {

}
void MeshView::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	const Ray& ray = mInput->GetPointer(0)->mWorldRay;
	float hitT;
	RaycastReceiver* hit = mScene->Raycast(ray, hitT);

	Gizmos* gizmos = mScene->Gizmos();

	// grid
	int size = 100;
	gizmos->DrawLine(float3(-size, 0, 0), float3(size, 0, 0), float4(1, 0, 0, .2f));
	gizmos->DrawLine(float3(0, -size, 0), float3(0, size, 0), float4(0, 1, 0, .2f));
	gizmos->DrawLine(float3(0, 0, -size), float3(0, 0, size), float4(0, 0, 1, .2f));
	for (int x = -size; x <= size; x++)
		if (x != 0) gizmos->DrawLine(float3(x, 0, -size), float3(x, 0, size), float4(1, 1, 1, x % 10 == 0 ? .1f : .025f));
	for (int z = -size; z <= size; z++)
		if (z != 0) gizmos->DrawLine(float3(-size, 0, z), float3(size, 0, z), float4(1, 1, 1, z % 10 == 0 ? .1f : .025f));

	bool change = mInput->GetPointer(0)->mAxis.at(0) > .5f;

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