#pragma once

#include <string>
#include <chrono>
#include <deque>
#include <vector>
#include <mutex>

#include <SFML/System/String.hpp>

#include "conc_lock.h"

namespace ts {

enum armory_raid_achievement_type {
	mythic_boss,
	meta
};

struct armory_raid_achievements
{
	unsigned int m_achievement_id;
	armory_raid_achievement_type m_achievement_type {mythic_boss};
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

struct wow_raid
{
	sf::String m_raid_name;
	sf::String m_icon_path;
	std::vector<armory_raid_achievements> m_armory_achievements;
	std::vector<armory_boss_statistics> m_armory_statistics;

	unsigned int get_armory_num_killed_bosses_acc() const;
	unsigned int get_total_bosses() const;
};

inline unsigned int wow_raid::get_armory_num_killed_bosses_acc() const {
	unsigned int killed_bosses = 0;
	for (const auto& achievement : m_armory_achievements) {
		if (achievement.m_achievement_type == armory_raid_achievement_type::mythic_boss && achievement.m_unlocked) {
			++killed_bosses;
		}
	}

	return killed_bosses;
}

inline unsigned int wow_raid::get_total_bosses() const {
	return m_armory_statistics.size();
}

class raid_data
{
public:
	raid_data();

	void cl_lock() const;
	void cl_unlock() const;

	// internally thread safe
	bool is_initialized() const;

	// returns nullptr if not found / not externally thread safe
	wow_raid* get_raid(const sf::String& raidname);
	
	// not externally thread safe
	std::deque<wow_raid>* get_raids();

private:
	void init_from_file();

	bool m_initialized {false};
	std::deque<wow_raid> m_raids;
	mutable std::mutex m_mutex;
};

inline void raid_data::cl_lock() const {
	m_mutex.lock();
}

inline void raid_data::cl_unlock() const {
	m_mutex.unlock();
}

inline bool raid_data::is_initialized() const {
	conc_lock<raid_data>(this);
	return m_initialized;
}

inline std::deque<wow_raid>* raid_data::get_raids() {
	return &m_raids;
}

} // namespace ts
