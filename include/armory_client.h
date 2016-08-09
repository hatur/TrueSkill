#pragma once

#include <memory>
#include <functional>
#include <vector>

#include <Poco/URI.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/JSONException.h>
#include <Poco/Timestamp.h>

#include "raid_data.h"

namespace ts {

class ts_central;

struct realm
{
	realm() {}
	realm(std::wstring country, std::wstring name)
		: m_country{std::move(country)}
		, m_name{std::move(name)} {}

	std::wstring m_country;
	std::wstring m_name;
};

class armory_client
{
public:
	armory_client(ts_central* central);

	std::vector<realm> retrieve_realm_list(std::string region);

	// may throw ts_exception on failure
	void retrieve_raid_data(std::string region, std::string server, std::string name, std::shared_ptr<raid_data> raiddata);

private:
	// @return 
	std::string send_mashery_request(const Poco::URI& uri);

	ts_central* m_central;
};

} // namespace ts