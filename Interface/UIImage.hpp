#pragma once

#include <Interface/UIElement.hpp>

#include <Content/Font.hpp>
#include <Content/Material.hpp>
#include <Core/Buffer.hpp>
#include <Core/DescriptorSet.hpp>

#include <cstring>

class UIImage : public UIElement {
public:
	ENGINE_EXPORT UIImage(const std::string& name);
	ENGINE_EXPORT ~UIImage();

	inline ::Texture* Texture() const { return mTexture.index() == 0 ? std::get<std::shared_ptr<::Texture>>(mTexture).get() : std::get<::Texture*>(mTexture); }
	inline void Texture(std::shared_ptr<::Texture> t) { mTexture = t; }
	inline void Texture(::Texture* t) { mTexture = t; }

	inline std::shared_ptr<::Material> Material() const { return mMaterial; }
	inline void Material(std::shared_ptr<::Material> m) { mMaterial = m; }

	inline void Color(const float4& c) { mColor = c; }
	inline float4 Color() const { return mColor; }

	ENGINE_EXPORT virtual void Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) override;

private:
	struct DeviceData {
		Buffer** mObjectBuffers;
		DescriptorSet** mDescriptorSets;
	};

	float4 mColor;
	std::variant<std::shared_ptr<::Texture>, ::Texture*> mTexture;
	std::shared_ptr<::Material> mMaterial;

	AABB mAABB;
	std::unordered_map<Device*, DeviceData> mDeviceData;
};