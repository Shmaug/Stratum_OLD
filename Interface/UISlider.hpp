#pragma once

#include <Interface/UIImage.hpp>
#include <cstring>

class UISlider : public UIElement {
public:
	ENGINE_EXPORT UISlider(const std::string& name, UICanvas* canvas);
	ENGINE_EXPORT ~UISlider();

	inline float4 OutlineColor() const { return mOutlineColor; }
	inline void OutlineColor(const float4& c) { mOutlineColor = c; }

	inline bool Outline() const { return mOutline; }
	inline void Outline(bool w) { mOutline = w; }

	inline void Color(const float4& c) { mColor = c; }
	inline float4 Color() const { return mColor; }

	ENGINE_EXPORT virtual void Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) override;

private:
	struct DeviceData {
		Buffer** mObjectBuffers;
		DescriptorSet** mDescriptorSets;
	};

    float mMinimum;
    float mMaximum;
    float mValue;

	Shader* mShader;
	bool mOutline;
	float4 mOutlineColor;
	float4 mColor;
	std::variant<std::shared_ptr<::Texture>, ::Texture*> mTexture;

	AABB mAABB;
	std::unordered_map<Device*, DeviceData> mDeviceData;
};