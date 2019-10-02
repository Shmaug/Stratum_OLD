#pragma once

#include <Interface/UIElement.hpp>

class VerticalLayout : UIElement {
public:
	ENGINE_EXPORT VerticalLayout(const std::string& name, UICanvas* canvas);
	ENGINE_EXPORT ~VerticalLayout();

	ENGINE_EXPORT bool AddChild(UIElement* element);
	// Update child positions (called in AddChild)
	ENGINE_EXPORT void UpdateLayout();

	// Vertical spacing between elements
	inline float Spacing() const { return mSpacing; }
	// Vertical spacing between elements
	inline void Spacing(float s) { mSpacing = s; UpdateLayout(); }

	// Padding on left/right side of elements
	inline float Padding() const { return mPadding; }
	// Padding on left/right side of elements
	inline void Padding(float p) { mPadding = p; UpdateLayout(); }

private:
	// Vertical spacing between elements
	float mSpacing;
	// Padding on left/right side of elements
	float mPadding;
};