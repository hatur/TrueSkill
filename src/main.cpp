#include <iostream>

#include <Poco/Net/DNS.h>
#include <SFML/Window.hpp>
#include <TGUI/TGUI.hpp>

#include "config.h"

int main() {
	// Poco Test

	const auto& entry = Poco::Net::DNS::hostByName("www.google.com");

	std::cout << "Resolving " << entry.name() << std::endl;

	const auto& aliases = entry.aliases();

	for (const auto& alias : aliases) {
		std::cout << "Alias: " << alias << std::endl;
	}

	const auto& addrList = entry.addresses();

	for (const auto& address : addrList) {
		std::cout << "Address: " << address.toString() << std::endl;
	}

	// SFML + TGUI Test
	sf::RenderWindow window{sf::VideoMode{1280, 720}, "TrueSkill - version " + ts::get_version(), sf::Style::None};

	tgui::Gui gui{window};

	auto button = std::make_shared<tgui::Button>();
	button->setText(sf::String("Hellö"));
	button->connect("pressed", [&window] { window.close(); });

	gui.add(button);

	while (window.isOpen()) {
		sf::Event windowEvent;
		while (window.pollEvent(windowEvent)) {
			if (windowEvent.type == sf::Event::Closed) {
				window.close();
			}

			gui.handleEvent(windowEvent);
		}

		window.clear();
		gui.draw();
		window.display();
	}

	return 0;
}