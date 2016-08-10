#pragma once

#include <exception>

#include <SFML/Window.hpp>
#include <TGUI/TGUI.hpp>

#include <mutex>

#include "ts_exception.h"
#include "conc_lock.h"
#include "tooltip_wrapper.h"

namespace ts {

class ts_central;

/**
* Abstract base wrapper class for the gui, exposes needed functions like ui transform, column additions etc.*
*
* Exposed methods are thread safe
**/
class gui
{
public:
	gui(ts_central* central);
	virtual ~gui() = default;

	gui(const gui&) = delete;
	gui& operator=(const gui&) = delete;
	gui(gui&&) = delete;
	gui& operator=(gui&&) = delete;

	void cl_lock() const;
	void cl_unlock() const;

	void init();

	// forwarding
	void draw();

	// forwarding
	void handle_event(sf::Event& windowEvent);

protected:
	virtual void build() = 0;

	void register_tooltip(std::unique_ptr<tooltip_wrapper> tooltip);

	// TODO: this is currently not thread safe and on insecure use, consider passing the variables directly as copy so no external access is needed
	// this is also pretty important!
	ts_central* get_central();

	std::recursive_mutex& get_mutex();
	tgui::Gui& get_gui();

private:
	void clear();
	
	ts_central* m_central;
	tgui::Gui m_gui;
	mutable std::recursive_mutex m_mutex;

	std::vector<std::unique_ptr<tooltip_wrapper>> m_tooltips;
};

inline void gui::cl_lock() const {
	m_mutex.lock();
}

inline void gui::cl_unlock() const {
	m_mutex.unlock();
}

inline void gui::register_tooltip(std::unique_ptr<tooltip_wrapper> tooltip) {
	m_tooltips.push_back(std::move(tooltip));
}

inline ts_central* gui::get_central() {
	return m_central;
}

inline std::recursive_mutex& gui::get_mutex() {
	return m_mutex;
}

inline tgui::Gui& gui::get_gui() {
	return m_gui;
}

} // namespace ts

