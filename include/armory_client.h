#pragma once

#include <string>

#include <SFML/System/String.hpp>
#include <Poco/URI.h>

#include "raid_data.h"

namespace ts {

struct realm
{
	realm() {}
	realm(std::wstring country, std::wstring name)
		: m_country{std::move(country)}
		, m_name{std::move(name)} {}

	sf::String m_country;
	sf::String m_name;
};

class armory_client
{
public:
	armory_client() = delete;

	// @brief may throw ts_exception on failure, not internally thread safe
	static std::vector<realm> retrieve_realm_list(std::string region);

	// @brief throw ts_exception on failure, not internally thread safe
	static void retrieve_raid_data(sf::String region, sf::String server, sf::String name, raid_data& io_raiddata_copy);

private:
	// @brief internally used as central hub to send requests
	// @return content of request
	static std::string send_mashery_request(const Poco::URI& uri);
};

} // namespace ts