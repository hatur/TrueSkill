#include "..\include\tooltip_wrapper.h"

namespace ts {

tooltip_wrapper::tooltip_wrapper(tgui::Gui* gui, std::shared_ptr<tgui::Widget> linked_widget, std::shared_ptr<tgui::Widget> tooltip_widget)
	: m_gui{gui}
	, m_linked_widget{linked_widget}
	, m_tooltip_widget{tooltip_widget} {

	Expects(m_gui != nullptr);
	Expects(m_linked_widget != nullptr);
	Expects(m_tooltip_widget != nullptr);

	m_tooltip_widget->hide();

	m_linked_widget->connect("MouseEntered", [this] {
		if (m_tooltip_shown == false) { // we need this because of the bug described in update()
			auto mouse_pos = sf::Mouse::getPosition(*(dynamic_cast<sf::RenderWindow*>(m_gui->getWindow())));
			auto view_pos = m_gui->getWindow()->mapPixelToCoords(mouse_pos);

			m_tooltip_widget->setPosition(sf::Vector2f{view_pos.x, view_pos.y - m_tooltip_widget->getSize().y});
			m_tooltip_widget->moveToFront();
			m_tooltip_widget->show();
			m_tooltip_shown = true;
		}
	});
	//m_linked_widget->connect("MouseLeft", [this] {
	//	// So, for some reason mouseLeft is bugged in tgui when moved upwards in the widget, we do in manually in update
	//});

	m_gui->add(tooltip_widget);
}

void tooltip_wrapper::update() {
	// While this may be pretty expensive it is the only safe way to do so since mouseLeft for tgui is bugged if the mouse is moved upwards inside the widget
	if (m_tooltip_shown) {
		auto mouse_pos = sf::Mouse::getPosition(*(dynamic_cast<sf::RenderWindow*>(m_gui->getWindow())));
		auto view_pos = m_gui->getWindow()->mapPixelToCoords(mouse_pos);

		sf::Vector2f absolute_pos{0.f, 0.f};

		tgui::Widget* current_widget = m_linked_widget.get();
		while (true) {
			absolute_pos += current_widget->getPosition();
			current_widget = current_widget->getParent();
			if (current_widget == nullptr) {
				break;
			}
		}

		auto linked_widget_size = m_linked_widget->getSize();

		if (view_pos.x >= absolute_pos.x && view_pos.x <= absolute_pos.x + linked_widget_size.x
			&& view_pos.y >= absolute_pos.y && view_pos.y <= absolute_pos.y + linked_widget_size.y) {
			m_tooltip_widget->setPosition(sf::Vector2f{view_pos.x, view_pos.y - m_tooltip_widget->getSize().y});
		}
		else {
			m_tooltip_widget->hide();
			m_tooltip_shown = false;
		}	
	}
}

} // namespace ts