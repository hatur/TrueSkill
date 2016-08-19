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
};

static sf::String send_warcraftlogs_request(const Poco::URI& uri);

} // namespace ts

