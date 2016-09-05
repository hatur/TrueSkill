#pragma once

#include "gui.h"
#include "raid_data.h"

#include "armory_client.h"
#include "warcraftlogs_client.h"

namespace ts {

class ts_central;
struct realm;

class main_gui : public gui
{
public:
	main_gui(ts_central* central);

	// @brief logs a message to the chatbox, internally thread safe
	void log_message(sf::String message);

private:
	void build() override;

	// @info thread safe
	void push_realm_list(std::vector<realm>&& realmlist);

	void build_raid_layout(sf::String region, sf::String server, sf::String name, std::vector<std::pair<unsigned int, tgui::CheckBox::Ptr>> wcl_raids, std::vector<metric> wcl_metrics);

	// @brief helper function to fill statistics tooltips
	// @info not thread safe, not internally acquiring any locks!
	void fill_armory_character_statistics_tooltip(tgui::Panel* tooltip, const wow_raid& raid, boss_difficulty bossdifficulty);
	void clear_raid_layout();

	void build_wait_layout();
	void remove_wait_layout();

	void push_wait_message(sf::String message);

	void calculate_and_fill_top_percentages(raid_data& raiddata, raid_data& wcl_raiddata_rankings_copy, const std::vector<unsigned int>& wcl_selected_raids);

	std::shared_ptr<tgui::Theme> m_tgui_theme{nullptr};
	std::unique_ptr<raid_data> m_cached_raiddata{nullptr};
	std::atomic<bool> m_request_ongoing{false};
	std::vector<realm> m_cached_realmlist;
	tgui::ChatBox::Ptr m_log_box{nullptr};
};

//std::pair<bool, float> calculate_avg_pct(std::vector<float> content);

// free functions

} // namespace ts