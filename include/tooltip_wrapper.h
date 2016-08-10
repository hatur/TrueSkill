#pragma once

#include <gsl/gsl.h>

#include <SFML/Window/Mouse.hpp>

#include <TGUI/Widget.hpp>
#include <TGUI/Gui.hpp>

namespace ts {

class tooltip_wrapper
{
public:
	tooltip_wrapper(tgui::Gui* gui, std::shared_ptr<tgui::Widget> linked_widget, std::shared_ptr<tgui::Widget> tooltip_widget);
	~tooltip_wrapper();

	void update();

private:
	tgui::Gui* m_gui;
	std::shared_ptr<tgui::Widget> m_linked_widget;
	std::shared_ptr<tgui::Widget> m_tooltip_widget;

	bool m_tooltip_shown = false;
};

inline tooltip_wrapper::~tooltip_wrapper() {
	m_gui->remove(m_tooltip_widget);
}

} // namespace ts

