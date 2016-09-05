#include "ts_central.h"
#include "gui.h"

namespace ts {

gui::gui(ts_central* central)
	: m_central{central}
	, m_gui{*central->get_renderwindow()} {

	{
		auto lock = conc_lock<gui>(this);
		m_gui.setFont("../data/fonts/DroidSans.ttf");
	}
}

void gui::init() {
	clear();
	build();
}

void gui::draw() {
	auto lock = conc_lock<gui>{this};

	for (const auto& tooltip : m_tooltips) {
		tooltip->update();
	}

	m_gui.draw();
}

void gui::handle_event(sf::Event& window_event) {
	auto lock = conc_lock<gui>{this};

	try {
		m_gui.handleEvent(window_event);
	}
	catch (tgui::Exception& ex) {
		std::cout << "GUI Exception: " << ex.what() << std::endl;
	}
}

void gui::clear() {
	auto lock = conc_lock<gui>{this};
	m_gui.removeAllWidgets();
}

//void gui::build_test_screen() {
//	std::lock_guard<std::recursive_mutex> lock{m_mutex};
//
//	auto button = std::make_shared<tgui::Button>();
//	button->setText(sf::String("Switch"));
//	button->setPosition(0, m_central->get_renderwindow()->getSize().y - button->getSize().y);
//	button->connect("pressed", [this] { show(gui_type::main_menu); });
//
//	m_gui.add(button);
//}

} // namespace ts