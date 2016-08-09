#include "raid_data.h"

#include <iostream>
#include <fstream>
#include <locale>
#include <codecvt>

#include <Poco/JSON/Parser.h>
#include <Poco/JSON/JSONException.h>
#include <Poco/Dynamic/Var.h>

namespace ts {

raid_data::raid_data() {
	init_from_file();
}

void raid_data::init_from_file() {
	using namespace Poco;
	using namespace Poco::JSON;

	auto lock = conc_lock<raid_data>(this);

	m_initialized = false;

	std::ifstream file{"data/raid_data.json", std::ios::in | std::ios::binary};
	file.imbue(std::locale{std::locale::classic(), new std::codecvt_utf8<wchar_t>});

	if (!file.is_open()) {
		std::cout << "raid_data::init_from_file(): couldn't open data/raid_data.json" << std::endl;
		return;
	}

	std::stringstream ss;
	ss << file.rdbuf();

	std::cout << "Content of json: " << std::endl << ss.str() << std::endl;

	try {
		Parser parser;
		auto result = parser.parse(ss.str());

		auto json_object = result.extract<Object::Ptr>();

		auto raids = json_object->get("raids").extract<Array::Ptr>();

		std::cout << "Found " << raids->size() << " raids" << std::endl;

		for (const auto& raid : *raids) {
			auto raid_data = raid.extract<Object::Ptr>();
			std::cout << "loading data for raid with name: " << raid_data->get("raid_name").extract<std::string>() << std::endl;

			wow_raid wowraid;
			wowraid.m_raid_name = raid_data->get("raid_name").extract<std::string>();
			wowraid.m_icon_path = std::string{"data/img/" + raid_data->get("raid_icon_name").extract<std::string>()};
			
			auto bosses = raid_data->get("raid_bosses").extract<Array::Ptr>();
			for (const auto& boss : *bosses) {
				auto boss_data = boss.extract<Object::Ptr>();
				std::cout << "* boss: " << boss_data->get("boss_name").extract<std::string>() << std::endl;

				armory_boss_statistics boss_statistics{};
				boss_statistics.m_boss_name = boss_data->get("boss_name").extract<std::string>();

				wowraid.m_armory_statistics.push_back(std::move(boss_statistics));
			}

			auto achievements = raid_data->get("raid_achievements").extract<Array::Ptr>();
			for (const auto& achievement : *achievements) {
				auto achievement_data = achievement.extract<Object::Ptr>();

				armory_raid_achievements ach_data{};

				auto ach_type = achievement_data->get("type").extract<std::string>();

				if (ach_type == "mythic_boss_ach") {
					ach_data.m_achievement_type = armory_raid_achievement_type::mythic_boss;
				}
				else if (ach_type == "meta_ach") {
					ach_data.m_achievement_type = armory_raid_achievement_type::meta;
				}
				else {
					std::cout << "raid_data::init_from_file(): Illegal achievement type detected!" << std::endl;
					continue;
				}

				auto ach_id = achievement_data->get("ach_id").extract<int32_t>();
				ach_data.m_achievement_id = static_cast<unsigned int>(ach_id);

				auto ach_name = achievement_data->get("ach_name").extract<std::string>();
				ach_data.m_achievement_name = sf::String{ach_name};

				std::cout << "* achievement: " << ach_id << " (type: " << ach_type << ")" << std::endl;

				wowraid.m_armory_achievements.push_back(std::move(ach_data));
			}

			m_raids.push_back(std::move(wowraid));

			m_initialized = true;
		}
	}
	catch (Exception& ex) {
		std::cout << "Error parsing raid data, reason: " << ex.what() << std::endl;
		m_initialized = false;
	}
}

wow_raid* raid_data::get_raid(const sf::String& raid_name) {
	auto it = std::find_if(m_raids.begin(), m_raids.end(), [&raid_name] (wow_raid& raid) {
		return raid.m_raid_name == raid_name;
	});

	if (it == m_raids.end()) {
		return nullptr;
	}

	return &(*it);
}

} // namespace ts
