#pragma once

#include <Interface/UIElement.hpp>

#include <Content/Font.hpp>
#include <Content/Material.hpp>
#include <Core/Buffer.hpp>
#include <Core/DescriptorSet.hpp>

#include <cstring>

class UIImage : public UIElement {
public:
	ENGINE_EXPORT UIImage(const std::string& name, UICanvas* canvas);
	ENGINE_EXPORT ~UIImage();

	inline float4 OutlineColor() const { return mOutlineColor; }
	inline void OutlineColor(const float4& c) { mOutlineColor = c; }

	inline bool Outline() const { return mOutline; }
	inline void Outline(bool w) { mOutline = w; }

	inline ::Texture* Texture() const { return mTexture.index() == 0 ? std::get<std::shared_ptr<::Texture>>(mTexture).get() : std::get<::Texture*>(mTexture); }
	inline void Texture(std::shared_ptr<::Texture> t) { mTexture = t; }
	inline void Texture(::Texture* t) { mTexture = t; }

	inline void Color(const float4& c) { mColor = c; }
	inline float4 Color() const { return mColor; }

	ENGINE_EXPORT virtual void Draw(CommandBuffer* commandBuffer, Camera* camera, ::Material* materialOverride) override;

private:
	struct DeviceData {
		DescriptorSet** mDescriptorSets;
	};

	Shader* mShader;
	bool mOutline;
	float4 mOutlineColor;
	float4 mColor;
	std::variant<std::shared_ptr<::Texture>, ::Texture*> mTexture;

	AABB mAABB;
	std::unordered_map<Device*, DeviceData> mDeviceData;
};