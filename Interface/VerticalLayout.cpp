#include <Interface/VerticalLayout.hpp>

using namespace std;

VerticalLayout::VerticalLayout(const string& name, UICanvas* canvas) : UIElement(name, canvas), mSpacing(0.f) {}
VerticalLayout::~VerticalLayout() {}

bool VerticalLayout::AddChild(UIElement* e) {
	if (!UIElement::AddChild(e)) return false;
	UpdateLayout();
	return true;
}

void VerticalLayout::UpdateLayout() {
	vec2 c = vec2(mPadding, mSpacing);
	for (uint32_t i = 0; i < ChildCount(); i++) {
		UIElement* e = Child(i);
		UDim2 p = e->Position();
		p.mOffset = c;
		p.mScale = vec2(0);
		e->Position(p);

		UDim2 s = e->Extent();
		s.mScale = vec2(0, 1);
		s.mOffset.x = -mPadding * 2.f;
		e->Extent(s);

		c.y += s.mOffset.y + mSpacing;
	}
}