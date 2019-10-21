#include <Interface/VerticalLayout.hpp>

using namespace std;

VerticalLayout::VerticalLayout(const string& name, UICanvas* canvas) : UIElement(name, canvas), mSpacing(0.f) {}
VerticalLayout::~VerticalLayout() {}

void VerticalLayout::UpdateLayout() {
	float sy = 1;
	float oy = 0;
	for (uint32_t i = 0; i < ChildCount(); i++) {
		UIElement* e = Child(i);
		UDim2 p = e->Position();
		p.mScale.y = sy;
		p.mOffset.y = oy;
		e->Position(p);
		oy -= e->AbsoluteExtent().y * 2.f + mSpacing;
	}
}