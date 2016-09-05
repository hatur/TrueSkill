#pragma once

#include <SFML/System/String.hpp>
#include <Poco/URI.h>

#include "thread_pool.h"
#include "raid_data.h"

namespace ts {

struct wcl_job_information
{
	unsigned int m_zone_id;
	metric m_metric;
};

struct wcl_raid_bracket
{
	unsigned int m_id;
	//unsigned int m_min_ilvl;
	//unsigned int m_max_ilvl;
	sf::String m_bracket_name;
};

struct wcl_raid_bracket_raid
{
	unsigned int m_wcl_raid_id;
	std::vector<wcl_raid_bracket> m_brackets;
};

class warcraftlogs_client
{
public:
	warcraftlogs_client() = delete;

	// @brief retrieves top rankings from warcraftlogs - overall or per bracket (not yet implemented)
	static void retrieve_ranking_log_data(sf::String region, sf::String server, sf::String name, std::vector<unsigned int> wcl_selected_raids, std::vector<metric> wcl_metrics,
		thread_pool& threadpool, raid_data& io_raiddata_copy);

	// @brief retrieves all parse data, very slow.. used to create median data overall or per bracket (not yet implemented)
	static void retrieve_parse_log_data(sf::String region, sf::String server, sf::String name, std::vector<unsigned int> wcl_selected_raids, std::vector<metric> wcl_metrics,
		thread_pool& threadpool, raid_data& io_raiddata_copy);
	
private:
	// @brief internal
	static std::vector<wcl_raid_bracket_raid> retrieve_bracket_information(const std::vector<unsigned int>& wcl_selected_raids);
};

static sf::String send_warcraftlogs_request(const Poco::URI& uri);

} // namespace ts

