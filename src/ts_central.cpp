#include "ts_central.h"

#include <iostream>

#include <Poco/Net/DNS.h>
#include <SFML/Window.hpp>

#include "config.h"
#include "thread_pool.h"
#include "main_gui.h"

namespace ts {

ts_central::ts_central() {
	// Poco Test

	//std::cout << "Testing http connection to google.com" << std::endl;

	//const auto& entry = Poco::Net::DNS::hostByName("www.google.com");

	//std::cout << "Resolving " << entry.name() << std::endl;

	//const auto& aliases = entry.aliases();

	//for (const auto& alias : aliases) {
	//	std::cout << "Alias: " << alias << std::endl;
	//}

	//const auto& addrList = entry.addresses();

	//for (const auto& address : addrList) {
	//	std::cout << "Address: " << address.toString() << std::endl;
	//}

	//std::cout << std::endl;

	m_threadpool = std::make_unique<thread_pool>();

	// SFML + TGUI Test
	m_renderwindow = std::make_unique<sf::RenderWindow>(sf::VideoMode{1280, 720}, "TrueSkill - version " + config::get_version(), sf::Style::Default);
	m_renderwindow->setFramerateLimit(30);

	m_gui = std::make_unique<main_gui>(this);
	m_gui->init();

	while (m_renderwindow->isOpen()) {
		sf::Event windowEvent;
		while (m_renderwindow->pollEvent(windowEvent)) {
			if (windowEvent.type == sf::Event::Closed) {
				m_renderwindow->close();
			}

			m_gui->handle_event(windowEvent);
		}

		m_renderwindow->clear();
		m_gui->draw();
		m_renderwindow->display();
	}
}

} // namespace ts