#include <Interface/VerticalLayout.hpp>

using namespace std;

VerticalLayout::VerticalLayout(const string& name) : UIElement(name), mSpacing(0.f) {}
VerticalLayout::~VerticalLayout() {}

bool VerticalLayout::AddChild(UIElement* e) {
	if (!UIElement::AddChild(e)) return false;
	UpdateLayout();
	return true;
}

void VerticalLayout::UpdateLayout() {
	vec2 c = vec2(1.f, 0);
	for (uint32_t i = 0; i < ChildCount(); i++) {
		UIElement* e = Child(i);
		UDim2 p = e->Position();
		p.mOffset.y = c.x;
		p.mScale.y = c.y;
		e->Position(p);
		c.x -= e->Extent().mScale.y;
		c.y -= e->Extent().mOffset.y + mSpacing;
	}
}