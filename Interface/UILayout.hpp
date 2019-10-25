#pragma once

#include <Interface/UIElement.hpp>

class UILayout : public UIElement {
public:
	ENGINE_EXPORT UILayout(const std::string& name, UICanvas* canvas);
	ENGINE_EXPORT ~UILayout();

	// Update child positions
	ENGINE_EXPORT void UpdateLayout();

	// Vertical spacing between elements
	inline float Spacing() const { return mSpacing; }
	// Vertical spacing between elements
	inline void Spacing(float s) { mSpacing = s; UpdateLayout(); }

private:
	// Vertical spacing between elements
	float mSpacing;
};