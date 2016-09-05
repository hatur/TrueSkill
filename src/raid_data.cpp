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

	m_initialized = false;

	std::ifstream file{"../data/raid_data.json", std::ios::in | std::ios::binary};
	file.imbue(std::locale{std::locale::classic(), new std::codecvt_utf8<wchar_t>});

	if (!file.is_open()) {
		std::cout << "raid_data::init_from_file(): couldn't open data/raid_data.json" << std::endl;
		return;
	}

	std::stringstream ss;
	ss << file.rdbuf();

	//std::cout << "Content of json: " << std::endl << ss.str() << std::endl;

	try {
		Parser parser;
		auto result = parser.parse(ss.str());

		auto json_object = result.extract<Object::Ptr>();

		auto raids = json_object->get("raids").extract<Array::Ptr>();

		//std::cout << "Found " << raids->size() << " raids" << std::endl;

		for (const auto& raid : *raids) {
			auto raid_data = raid.extract<Object::Ptr>();
			//std::cout << "loading data for raid with name: " << raid_data->get("raid_name").extract<std::string>() << std::endl;

			wow_raid wowraid;
			wowraid.m_raid_name = raid_data->get("raid_name").extract<std::string>();
			wowraid.m_default_raid = raid_data->get("raid_default_raid").extract<int>() == 1;
			wowraid.m_icon_path = std::string{"../data/wow_img/" + raid_data->get("raid_icon_name").extract<std::string>()};
			wowraid.m_wcl_zone_id = static_cast<unsigned int>(raid_data->get("raid_wcl_id").extract<int>());
			
			auto bosses = raid_data->get("raid_bosses").extract<Array::Ptr>();
			for (const auto& boss : *bosses) {
				auto boss_data = boss.extract<Object::Ptr>();

				const auto wcl_boss_id = static_cast<unsigned int>(boss_data->get("boss_wcl_id").extract<int>());
				const auto boss_name = boss_data->get("boss_name").extract<std::string>();

				wowraid.m_wcl_bosses.insert(std::make_pair(boss_name, wcl_boss_id));

				armory_boss_statistics boss_statistics{};
				boss_statistics.m_boss_name = boss_name;

				wowraid.m_armory_boss_statistics.push_back(std::move(boss_statistics));
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

				//std::cout << "* achievement: " << ach_id << " (type: " << ach_type << ")" << std::endl;

				wowraid.m_armory_achievements.push_back(std::move(ach_data));
			}

			m_raids.push_back(std::move(wowraid));

			m_initialized = true;
		}
	}
	catch (Exception& ex) {
		std::cout << "Error parsing raid data, reason: " << ex.displayText() << std::endl;
		throw ts_exception{std::string{"Couldn't initialize raid data, reason: " + ex.displayText()}.c_str()};
		m_initialized = false;
	}
}

wow_raid* raid_data::get_raid(const sf::String& raid_name) {
	auto it = std::find_if(m_raids.begin(), m_raids.end(), [&raid_name] (wow_raid& raid) {
		return raid.m_raid_name == raid_name;
	});

	if (it == m_raids.end()) {
		throw ts_exception("raid_data::get_raid(sf::String): raid not found");
	}

	return &(*it);
}

wow_raid* raid_data::get_raid(unsigned int zone_id) {
	auto it = std::find_if(m_raids.begin(), m_raids.end(), [zone_id] (wow_raid& raid) {
		return raid.m_wcl_zone_id == zone_id;
	});

	if (it == m_raids.end()) {
		throw ts_exception("raid_data::get_raid(uint): raid not found");
	}

	return &(*it);
}

unsigned int wow_raid::get_armory_num_killed_bosses_acc() const noexcept {
	unsigned int killed_bosses = 0;
	for (const auto& achievement : m_armory_achievements) {
		if (achievement.m_achievement_type == armory_raid_achievement_type::mythic_boss && achievement.m_unlocked) {
			++killed_bosses;
		}
	}

	return killed_bosses;
}

unsigned int wow_raid::get_armory_num_killed_bosses_char_nhc() const noexcept {
	unsigned int killed_bosses = 0;
	for (const auto& statistic : m_armory_boss_statistics) {
		if (statistic.m_num_nhc_kills > 0) {
			++killed_bosses;
		}
	}

	return killed_bosses;
}

unsigned int wow_raid::get_armory_num_killed_bosses_char_hc() const noexcept {
	unsigned int killed_bosses = 0;
	for (const auto& statistic : m_armory_boss_statistics) {
		if (statistic.m_num_hc_kills > 0) {
			++killed_bosses;
		}
	}

	return killed_bosses;
}

unsigned int wow_raid::get_armory_num_killed_bosses_char_m() const noexcept {
	unsigned int killed_bosses = 0;
	for (const auto& statistic : m_armory_boss_statistics) {
		if (statistic.m_num_m_kills > 0) {
			++killed_bosses;
		}
	}

	return killed_bosses;
}

bool warcraftlogs_top_rankings::has_avg_pct(metric _metric, boss_difficulty bossdifficulty, bool bracket) const {
	if (bossdifficulty == boss_difficulty::lfr || bossdifficulty == boss_difficulty::normal) {
		throw ts_exception{"warcraftlogs_top_rankings::get_avg_pct(): Unimplemented boss difficulty"};
	}

	switch (_metric) {
		case metric::DPS:
			if (bossdifficulty == boss_difficulty::heroic) {
				if (bracket) {
					return m_top_bracket_dps_hc.size() > 0;
				}
				else {
					return m_top_overall_dps_hc.size() > 0;
				}
			}
			else if (bossdifficulty == boss_difficulty::mythic) {
				if (bracket) {
					return m_top_bracket_dps_m.size() > 0;
				}
				else {
					return m_top_overall_dps_m.size() > 0;
				}
			}
			break;
		case metric::HPS:
			if (bossdifficulty == boss_difficulty::heroic) {
				if (bracket) {
					return m_top_bracket_hps_hc.size() > 0;
				}
				else {
					return m_top_overall_hps_hc.size() > 0;
				}
			}
			else if (bossdifficulty == boss_difficulty::mythic) {
				if (bracket) {
					return m_top_bracket_hps_m.size() > 0;
				}
				else {
					return m_top_overall_hps_m.size() > 0;
				}
			}
			break;
		case metric::KRSI:
			if (bossdifficulty == boss_difficulty::heroic) {
				if (bracket) {
					return m_top_bracket_krsi_hc.size() > 0;
				}
				else {
					return m_top_overall_krsi_hc.size() > 0;
				}
			}
			else if (bossdifficulty == boss_difficulty::mythic) {
				if (bracket) {
					return m_top_bracket_krsi_m.size() > 0;
				}
				else {
					return m_top_overall_krsi_m.size() > 0;
				}
			}
			break;
	}

	Ensures(false);
	return false;
}

float warcraftlogs_top_rankings::get_avg_pct(metric _metric, boss_difficulty bossdifficulty, bool bracket) const {
	if (bossdifficulty == boss_difficulty::lfr || bossdifficulty == boss_difficulty::normal) {
		throw ts_exception{"warcraftlogs_top_rankings::get_avg_pct(): Unimplemented boss difficulty"};
	}

	switch (_metric) {
		case metric::DPS:
			if (bossdifficulty == boss_difficulty::heroic) {
				if (bracket) {
					return std::accumulate(m_top_bracket_dps_hc.begin(), m_top_bracket_dps_hc.end(), 0.f) / static_cast<float>(m_top_bracket_dps_hc.size());
				}
				else {
					return std::accumulate(m_top_overall_dps_hc.begin(), m_top_overall_dps_hc.end(), 0.f) / static_cast<float>(m_top_overall_dps_hc.size());
				}
			}
			else if (bossdifficulty == boss_difficulty::mythic) {
				if (bracket) {
					return std::accumulate(m_top_bracket_dps_m.begin(), m_top_bracket_dps_m.end(), 0.f) / static_cast<float>(m_top_bracket_dps_m.size());
				}
				else {
					return std::accumulate(m_top_overall_dps_m.begin(), m_top_overall_dps_m.end(), 0.f) / static_cast<float>(m_top_overall_dps_m.size());
				}
			}
			break;
		case metric::HPS:
			if (bossdifficulty == boss_difficulty::heroic) {
				if (bracket) {
					return std::accumulate(m_top_bracket_hps_hc.begin(), m_top_bracket_hps_hc.end(), 0.f) / static_cast<float>(m_top_bracket_hps_hc.size());
				}
				else {
					return std::accumulate(m_top_overall_hps_hc.begin(), m_top_overall_hps_hc.end(), 0.f) / static_cast<float>(m_top_overall_hps_hc.size());
				}
			}
			else if (bossdifficulty == boss_difficulty::mythic) {
				if (bracket) {
					return std::accumulate(m_top_bracket_hps_m.begin(), m_top_bracket_hps_m.end(), 0.f) / static_cast<float>(m_top_bracket_hps_m.size());
				}
				else {
					return std::accumulate(m_top_overall_hps_m.begin(), m_top_overall_hps_m.end(), 0.f) / static_cast<float>(m_top_overall_hps_m.size());
				}
			}
			break;
		case metric::KRSI:
			if (bossdifficulty == boss_difficulty::heroic) {
				if (bracket) {
					return std::accumulate(m_top_bracket_krsi_hc.begin(), m_top_bracket_krsi_hc.end(), 0.f) / static_cast<float>(m_top_bracket_krsi_hc.size());
				}
				else {
					return std::accumulate(m_top_overall_krsi_hc.begin(), m_top_overall_krsi_hc.end(), 0.f) / static_cast<float>(m_top_overall_krsi_hc.size());
				}
			}
			else if (bossdifficulty == boss_difficulty::mythic) {
				if (bracket) {
					return std::accumulate(m_top_bracket_krsi_m.begin(), m_top_bracket_krsi_m.end(), 0.f) / static_cast<float>(m_top_bracket_krsi_m.size());
				}
				else {
					return std::accumulate(m_top_overall_krsi_m.begin(), m_top_overall_krsi_m.end(), 0.f) / static_cast<float>(m_top_overall_krsi_m.size());
				}
			}
			break;
	}

	Ensures(false);
	return 0.f;
}

wcl_top_overall_pct_information get_top_overall_avg_pct(const std::vector<warcraftlogs_top_rankings>& top_rankings, metric _metric, boss_difficulty bossdifficulty, bool bracket) {
	float accumulated_percentage = 0.f;
	unsigned int num_bosses = 0;

	for (const auto& boss_ranking : top_rankings) {
		if (boss_ranking.has_avg_pct(_metric, bossdifficulty, bracket)) {
			accumulated_percentage += boss_ranking.get_avg_pct(_metric, bossdifficulty, bracket);
			++num_bosses;
		}
	}

	wcl_top_overall_pct_information information{};
	information.m_has_value = num_bosses > 0;

	if (information.m_has_value) {
		const float avg_percentage = accumulated_percentage / static_cast<float>(num_bosses);

		information.m_float_value = avg_percentage;
		information.m_uint_value = static_cast<unsigned int>(std::round(avg_percentage));

		std::stringstream ss;
		ss << std::setprecision(number_of_digits(information.m_uint_value) + 2) << avg_percentage;

		information.m_rounded_string_value = sf::String{ss.str()};
	}

	return information;
}

} // namespace ts
