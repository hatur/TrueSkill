#pragma once

#include <string>
#include <chrono>
#include <deque>
#include <vector>
#include <mutex>

#include <SFML/System/String.hpp>
#include <unordered_map>

#include "conc_lock.h"
#include "ts_exception.h"

namespace ts {

enum class armory_raid_achievement_type {
	mythic_boss,
	meta
};

enum class boss_difficulty {
	lfr,
	normal,
	heroic,
	mythic
};

enum class metric {
	DPS,
	HPS,
	KRSI
};

struct armory_raid_achievements
{
	unsigned int m_achievement_id;
	armory_raid_achievement_type m_achievement_type {armory_raid_achievement_type::mythic_boss};
	sf::String m_achievement_name;
	bool m_unlocked {false};
	std::chrono::seconds m_unlock_timestamp;
	sf::String m_unlocktime_formatted;
};

struct armory_boss_statistics
{
	sf::String m_boss_name;

	unsigned int m_num_nhc_kills {0};
	unsigned int m_num_hc_kills {0};
	unsigned int m_num_m_kills {0};
};

struct warcraftlogs_top_rankings
{
	// @brief throws on wrong bossdifficulty (only works for heroic / mythic)
	bool has_avg_pct(metric _metric, boss_difficulty bossdifficulty, bool bracket) const;

	// @brief throws on wrong bossdifficulty (only works for heroic / mythic) - use has_avg_pct() to check if there is a value or else div 0 may happen
	float get_avg_pct(metric _metric, boss_difficulty bossdifficulty, bool bracket) const;

	sf::String m_boss_name;

	std::vector<float> m_top_bracket_dps_hc;
	std::vector<float> m_top_bracket_hps_hc;
	std::vector<float> m_top_bracket_krsi_hc;

	std::vector<float> m_top_overall_dps_hc;
	std::vector<float> m_top_overall_hps_hc;
	std::vector<float> m_top_overall_krsi_hc;

	std::vector<float> m_top_bracket_dps_m;
	std::vector<float> m_top_bracket_hps_m;
	std::vector<float> m_top_bracket_krsi_m;

	std::vector<float> m_top_overall_dps_m;
	std::vector<float> m_top_overall_hps_m;
	std::vector<float> m_top_overall_krsi_m;
};

struct warcraftlogs_median_rankings
{
	sf::String m_boss_name;

	float m_median_bracket_dps;
	float m_median_bracket_hps;
	float m_median_bracket_krsi;

	float m_median_overall_dps;
	float m_median_overall_hps;
	float m_median_overall_krsi;
};

struct wow_raid
{
	unsigned int get_armory_num_killed_bosses_acc() const noexcept;
	unsigned int get_armory_num_killed_bosses_char_nhc() const noexcept;
	unsigned int get_armory_num_killed_bosses_char_hc() const noexcept;
	unsigned int get_armory_num_killed_bosses_char_m() const noexcept;

	unsigned int get_total_bosses() const noexcept;

	// @brief throws ts_exception if boss not found
	unsigned int get_wcl_boss_id(sf::String bossname) const;

	// @brief throws ts_exception if id not found
	std::string get_wcl_boss_name(unsigned int boss_id) const;

	sf::String m_raid_name;
	bool m_default_raid;
	unsigned int m_wcl_zone_id;
	sf::String m_icon_path;
	std::unordered_map<std::string, unsigned int> m_wcl_bosses;

	std::vector<armory_raid_achievements> m_armory_achievements;
	std::vector<armory_boss_statistics> m_armory_boss_statistics;
	std::vector<warcraftlogs_top_rankings> m_wcl_per_boss_top_rankings;
	std::vector<warcraftlogs_median_rankings> m_wcl_per_boss_median_rankings;

	warcraftlogs_top_rankings m_calculated_top_rankings;
	warcraftlogs_median_rankings m_calculated_median_rankings;
};

inline unsigned int wow_raid::get_total_bosses() const noexcept {
	return m_armory_boss_statistics.size();
}

inline unsigned int wow_raid::get_wcl_boss_id(sf::String bossname) const {
	auto it = m_wcl_bosses.find(bossname);

	if (it == m_wcl_bosses.end()) {
		throw ts_exception("wow_raid::get_wcl_boss_id(): Boss not found");
	}

	return it->second;
}

inline std::string wow_raid::get_wcl_boss_name(unsigned int boss_id) const {
	auto it = std::find_if(m_wcl_bosses.begin(), m_wcl_bosses.end(), [boss_id] (auto& entry) {
		return entry.second == boss_id;
	});

	if (it == m_wcl_bosses.end()) {
		throw ts_exception("wow_raid::get_wcl_boss_name(): Boss not found");
	}

	return it->first;
}

class raid_data
{
public:
	raid_data();

	// internally thread safe
	bool is_initialized() const;

	// @brief throws ts_exception if name not found
	wow_raid* get_raid(const sf::String& raidname);

	// @brief throws ts_exception if name not found
	wow_raid* get_raid(unsigned int zone_id);
	
	// not externally thread safe
	std::deque<wow_raid>* get_raids();
	const std::deque<wow_raid>* get_raids() const;

private:
	void init_from_file();

	bool m_initialized {false};
	std::deque<wow_raid> m_raids;
};

//inline void raid_data::cl_lock() const {
//	m_mutex.lock();
//}
//
//inline void raid_data::cl_unlock() const {
//	m_mutex.unlock();
//}

inline bool raid_data::is_initialized() const {
	return m_initialized;
}

inline std::deque<wow_raid>* raid_data::get_raids() {
	return &m_raids;
}

inline const std::deque<wow_raid>* raid_data::get_raids() const {
	return &m_raids;
}

} // namespace ts
