#include <Interface/UILayout.hpp>

using namespace std;

UILayout::UILayout(const string& name, UICanvas* canvas) : UIElement(name, canvas), mSpacing(0.f) {}
UILayout::~UILayout() {}

void UILayout::UpdateLayout() {
	float sy = 1;
	float oy = 0;
	for (uint32_t i = 0; i < ChildCount(); i++) {
		UIElement* e = Child(i);
		if (!e->Visible()) continue;
		oy -= e->AbsoluteExtent().y + mSpacing;
		UDim2 p = e->Position();
		p.mScale.y = sy;
		p.mOffset.y = oy;
		e->Position(p);
		oy -= e->AbsoluteExtent().y + mSpacing;
	}
}