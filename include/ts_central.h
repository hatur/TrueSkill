#pragma once

#include "thread_pool.h"
#include "gui.h"

namespace ts {

class ts_central
{
public:
	ts_central();

	thread_pool* get_threadpool() const;
	gui* get_gui() const;
	sf::RenderWindow* get_renderwindow() const;

private:
	std::unique_ptr<gui> m_gui {nullptr};
	std::unique_ptr<sf::RenderWindow> m_renderwindow {nullptr};

	// This needs to be after all the other components because if gui etc shuts down before the thread pool we can't access it's members etc
	std::unique_ptr<thread_pool> m_threadpool{nullptr}; 
};

inline thread_pool* ts_central::get_threadpool() const {
	return m_threadpool.get();
}

inline gui* ts_central::get_gui() const {
	return m_gui.get();
}

inline sf::RenderWindow* ts_central::get_renderwindow() const {
	return m_renderwindow.get();
}

} // namespace ts