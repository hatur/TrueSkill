#include "warcraftlogs_client.h"

#include <iostream>
#include <locale>
#include <codecvt>
#include <fstream>

#include <Poco/Path.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/JSONException.h>
#include <Poco/Timestamp.h>
#include <Poco/Exception.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Dynamic/Var.h>
#include <Poco/UnicodeConverter.h>

#include "gsl/gsl.h"
#include "config.h"
#include "ts_exception.h"

namespace ts {

void warcraftlogs_client::retrieve_ranking_log_data(sf::String region, sf::String server, sf::String name, std::vector<unsigned int> wcl_selected_raids, std::vector<metric> wcl_metrics,
	thread_pool & threadpool, raid_data & io_raiddata_copy) {
	using namespace Poco;
	using namespace Poco::JSON;

	if (wcl_selected_raids.size() == 0) {
		throw ts_exception{"no raids selected"};
	}

	if (wcl_metrics.size() == 0) {
		throw ts_exception{"no metrics selected"};
	}

	auto utf8_name = std::string{};
	auto utf8_server = std::string{};

	Poco::UnicodeConverter::toUTF8(server.toWideString(), utf8_server);
	Poco::UnicodeConverter::toUTF8(name.toWideString(), utf8_name);

	auto bracket_information = retrieve_bracket_information(wcl_selected_raids);

	for (const auto& raid_bracket : bracket_information) {
		std::cout << "Bracket information for " << io_raiddata_copy.get_raid(raid_bracket.m_wcl_raid_id)->m_raid_name.toAnsiString() << ": " << std::endl;
		for (const auto& bracket : raid_bracket.m_brackets) {
			std::cout << bracket.m_id << ": " << bracket.m_bracket_name.toAnsiString() << std::endl;
		}

		if (raid_bracket.m_brackets.size() == 0) {
			std::cout << "-> No brackets found " << std::endl;
			io_raiddata_copy.get_raid(raid_bracket.m_wcl_raid_id)->m_has_brackets = false;
		}
	}

	//std::mutex per_raid_content_mutex;
	std::vector<std::pair<unsigned int, std::future<std::pair<wcl_job_information, sf::String>>>> jobs;
	std::vector<std::pair<wcl_job_information, std::string>> per_raid_content;

	unsigned int job_id = 0;
	for (const auto& raid : *io_raiddata_copy.get_raids()) {
		for (const auto _metric : wcl_metrics) {
			auto it = std::find(wcl_selected_raids.begin(), wcl_selected_raids.end(), raid.m_wcl_zone_id);

			if (it == wcl_selected_raids.end()) {
				continue;
			}

			static const auto metric_to_string_metric = [] (metric _metric) {
				switch (_metric) {
					case metric::DPS:
						return std::string{"dps"};
					case metric::HPS:
						return std::string{"hps"};
					case metric::KRSI:
						return std::string{"krsi"};
					default:
						abort();
						break;
				}
			};

			jobs.push_back(std::make_pair(job_id, std::async(std::launch::async, [utf8_name, utf8_server, region, zone_id(raid.m_wcl_zone_id), _metric] {
				URI uri;
				uri.setScheme("https");
				uri.setAuthority("www.warcraftlogs.com");
				uri.setPath("/v1/rankings/character/" + utf8_name + "/" + utf8_server + "/" + region.toAnsiString());
				uri.addQueryParameter("zone", std::to_string(zone_id));
				uri.addQueryParameter("metric", metric_to_string_metric(_metric));
				uri.addQueryParameter("api_key", "a44c1090cf2b560cc8306eeef9002fdc");

				//std::cout << "warcraftlogs_client::retrieve_ranking_log_data(): Request path: " << uri.getPathAndQuery() << std::endl;

				auto content = send_warcraftlogs_request(uri);

				wcl_job_information job_information;
				job_information.m_metric = _metric;
				job_information.m_zone_id = zone_id;

				return std::make_pair(std::move(job_information), std::move(content));
			})));

			++job_id;

			// If it is Hellfire Citadel, we need to get the default rankings and the pre-patch rankings
			if (raid.m_wcl_zone_id == 8) {
				jobs.push_back(std::make_pair(job_id, std::async(std::launch::async, [utf8_name, utf8_server, region, zone_id(raid.m_wcl_zone_id), _metric]{
					URI uri;
					uri.setScheme("https");
					uri.setAuthority("www.warcraftlogs.com");
					uri.setPath("/v1/rankings/character/" + utf8_name + "/" + utf8_server + "/" + region.toAnsiString());
					uri.addQueryParameter("zone", std::to_string(zone_id));
					uri.addQueryParameter("metric", metric_to_string_metric(_metric));
					uri.addQueryParameter("partition", std::to_string(2));
					uri.addQueryParameter("api_key", "a44c1090cf2b560cc8306eeef9002fdc");

					//std::cout << "warcraftlogs_client::retrieve_ranking_log_data(): Request path: " << uri.getPathAndQuery() << " (HFC partition 2)" << std::endl;

					auto content = send_warcraftlogs_request(uri);

					wcl_job_information job_information;
					job_information.m_metric = _metric;
					job_information.m_zone_id = zone_id;

					return std::make_pair(std::move(job_information), std::move(content));
				})));

				++job_id;
			}
		}
	}

	for (auto& job : jobs) {
		auto& pair = job.second.get();
		auto& job_information = pair.first;
		auto& content = pair.second;
		per_raid_content.push_back(std::make_pair(std::move(job_information), std::move(content)));
	}

	try {
		Parser parser;

		for (const auto& content : per_raid_content) {
			parser.reset();

			auto* raid = io_raiddata_copy.get_raid(content.first.m_zone_id);

			auto parse_result = parser.parse(content.second);
			const auto json_arr = parse_result.extract<Array::Ptr>();

			std::vector<warcraftlogs_top_rankings> wcl_top_rankings;

			//std::cout << "Start data for raid with id: " << content.first << std::endl;

			for (const auto& entry : *json_arr) {
				const auto entry_obj = entry.extract<Object::Ptr>();
				const auto encounter_id = static_cast<unsigned int>(entry_obj->get("encounter").extract<int>());

				const auto boss_name = sf::String{raid->get_wcl_boss_name(encounter_id)};
				const auto rank = entry_obj->get("rank").extract<int>();
				const auto out_of = entry_obj->get("outOf").extract<int>();
				const auto difficulty = entry_obj->get("difficulty").extract<int>();

				const auto bossdifficulty = [&difficulty] {
					if (difficulty == 4) {
						return boss_difficulty::heroic;
					}
					else if (difficulty == 5) {
						return boss_difficulty::mythic;
					}
					else {
						return boss_difficulty::lfr;
					}
				}();

				if (bossdifficulty != boss_difficulty::heroic && bossdifficulty != boss_difficulty::mythic) {
					continue;
				}

				float percentage = 100.f - (static_cast<float>(rank) * 100.f) / static_cast<float>(out_of);

				std::cout << "Parsed " << boss_name.toAnsiString() << ", calculated percentage: " << percentage << std::endl;

				auto it = std::find_if(raid->m_wcl_per_boss_top_rankings.begin(), raid->m_wcl_per_boss_top_rankings.end(), [&boss_name] (warcraftlogs_top_rankings& ranking) {
					return ranking.m_boss_name == boss_name;
				});

				warcraftlogs_top_rankings* ranking_ptr = nullptr;

				if (it == raid->m_wcl_per_boss_top_rankings.end()) {
					warcraftlogs_top_rankings wcl_ranking{};
					wcl_ranking.m_boss_name = boss_name;

					raid->m_wcl_per_boss_top_rankings.push_back(std::move(wcl_ranking));
					ranking_ptr = &raid->m_wcl_per_boss_top_rankings.back();
				}
				else {
					ranking_ptr = &(*it);
				}

				if (bossdifficulty == boss_difficulty::heroic) {
					switch (content.first.m_metric) {
						case metric::DPS:
							ranking_ptr->m_top_overall_dps_hc.push_back(percentage);
							break;
						case metric::HPS:
							ranking_ptr->m_top_overall_hps_hc.push_back(percentage);
							break;
						case metric::KRSI:
							ranking_ptr->m_top_overall_krsi_hc.push_back(percentage);
							break;
					}
				}
				else { // mythic
					switch (content.first.m_metric) {
						case metric::DPS:
							ranking_ptr->m_top_overall_dps_m.push_back(percentage);
							break;
						case metric::HPS:
							ranking_ptr->m_top_overall_hps_m.push_back(percentage);
							break;
						case metric::KRSI:
							ranking_ptr->m_top_overall_krsi_m.push_back(percentage);
							break;
					}
				}

				//std::cout << "finished report" << std::endl;
			}

			//std::cout << "End data for raid with id: " << content.first << std::endl;

			//auto& boss_name = raid->get_wcl_boss_name();

			//wcl_ranking.m_boss_name = sf::String{boss_name};

			//std::cout << "Pseudo parsed " << boss_name << std::endl;
		}
	}
	catch (const Poco::JSON::JSONException& ex) {
		throw ts_exception(ex.displayText().c_str());
	}
}

void warcraftlogs_client::retrieve_parse_log_data(sf::String region, sf::String server, sf::String name, std::vector<unsigned int> wcl_selected_raids, std::vector<metric> wcl_metrics,
	thread_pool& threadpool, raid_data& io_raiddata_copy) {
	using namespace Poco;
	using namespace Poco::JSON;

	if (wcl_selected_raids.size() == 0) {
		throw ts_exception{"no raids selected"};
	}

	if (wcl_metrics.size() == 0) {
		throw ts_exception{"no metrics selected"};
	}

	auto utf8_name = std::string{};
	auto utf8_server = std::string{};

	Poco::UnicodeConverter::toUTF8(server.toWideString(), utf8_server);
	Poco::UnicodeConverter::toUTF8(name.toWideString(), utf8_name);

	std::mutex per_raid_content_mutex;
	std::vector<std::pair<unsigned int, std::future<sf::String>>> jobs;
	std::vector<std::string> per_raid_content;

	unsigned int job_id = 0;
	for (const auto& raid : *io_raiddata_copy.get_raids()) {
		auto it = std::find(wcl_selected_raids.begin(), wcl_selected_raids.end(), raid.m_wcl_zone_id);

		if (it == wcl_selected_raids.end()) {
			continue;
		}

		jobs.push_back(std::make_pair(job_id, threadpool.job([utf8_name, utf8_server, region, zone_id(raid.m_wcl_zone_id)] {
			URI uri;
			uri.setScheme("https");
			uri.setAuthority("www.warcraftlogs.com");
			uri.setPath("/v1/parses/character/" + utf8_name + "/" + utf8_server + "/" + region.toAnsiString());
			uri.addQueryParameter("zone", std::to_string(zone_id));
			uri.addQueryParameter("api_key", "a44c1090cf2b560cc8306eeef9002fdc");

			//std::cout << "warcraftlogs_client::retrieve_parse_log_data(): Request path: " << uri.getPathAndQuery() << std::endl;

			return send_warcraftlogs_request(uri);
		})));
		++job_id;
	}

	for (auto& job : jobs) {
		auto content = job.second.get();
		per_raid_content.push_back(std::move(content));
	}

	try {

	}
	catch (const Poco::Exception& ex) {
		throw ts_exception(ex.displayText().c_str());
	}
}

std::vector<wcl_raid_bracket_raid> warcraftlogs_client::retrieve_bracket_information(const std::vector<unsigned int>& wcl_selected_raids) {
	using namespace Poco;
	using namespace Poco::JSON;

	std::vector<std::pair<unsigned int, std::future<sf::String>>> jobs;

	for (auto wcl_zone : wcl_selected_raids) {
		jobs.push_back(std::make_pair(wcl_zone, std::async(std::launch::async, [] {
			URI uri;
			uri.setScheme("https");
			uri.setAuthority("www.warcraftlogs.com");
			uri.setPath("/v1/zones");
			uri.addQueryParameter("api_key", "a44c1090cf2b560cc8306eeef9002fdc");
			
			return send_warcraftlogs_request(uri);
		})));
	}

	std::vector<wcl_raid_bracket_raid> raid_bracket_raids;

	for (auto& job : jobs) {
		auto& content = job.second.get();

		try {
			Parser parser;

			auto parse_result = parser.parse(content);
			const auto json_arr = parse_result.extract<Array::Ptr>();

			for (const auto& entry : *json_arr) {
				const auto entry_obj = entry.extract<Object::Ptr>();
				const auto entry_id = entry_obj->get("id").extract<int>();
				
				if (entry_id != job.first) {
					continue;
				}

				if (!entry_obj->has("brackets")) {
					std::cout << "Raid with id " << entry_id << " has no brackets!" << std::endl;
					continue;
				}

				wcl_raid_bracket_raid raid_bracket_raid{};
				raid_bracket_raid.m_wcl_raid_id = entry_id;

				const auto brackets = entry_obj->get("brackets").extract<Array::Ptr>();

				std::cout << "Brackets for zone with id " << job.first << std::endl;

				for (const auto& bracket : *brackets) {
					const auto bracket_obj = bracket.extract<Object::Ptr>();

					const auto bracket_id = bracket_obj->get("id").extract<int>();
					const auto bracket_name = bracket_obj->get("name").extract<std::string>();

					wcl_raid_bracket raid_bracket{};
					raid_bracket.m_id = bracket_id;
					raid_bracket.m_bracket_name = sf::String{std::move(bracket_name)};

					raid_bracket_raid.m_brackets.push_back(std::move(raid_bracket));
				}

				raid_bracket_raids.push_back(std::move(raid_bracket_raid));
			}
		}
		catch (const Poco::Exception& ex) {
			std::cout << "Error: Couldn't retrieve bracket information for zone with id: " << job.first << ", reason: " << ex.displayText() << std::endl;
			return std::vector<wcl_raid_bracket_raid>{};
		}
	}

	return raid_bracket_raids;
}

sf::String send_warcraftlogs_request(const Poco::URI& uri) {
	using namespace Poco::Net;

	auto path = uri.getPathAndQuery();
	if (path.empty()) {
		path = "/";
	}

	HTTPSClientSession session{uri.getHost(), uri.getPort(), ts::config::sslContext};

	std::string content;

	try {
		HTTPRequest request{HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1};
		request.setContentType("application/json;charset=UTF-8");
		session.sendRequest(request);

		HTTPResponse response;
		auto& is = session.receiveResponse(response);

		//std::cout << "send_warcraftlogs_request(): Status of response: " << response.getReason() << std::endl;
		//std::cout << "send_warcraftlogs_request(): Full response: " << std::endl;
		//response.write(std::cout);
		//std::cout << std::endl;

		if (response.getStatus() != HTTPResponse::HTTP_OK) {
			throw Poco::Exception{"Response didn't yield HTTP_OK"};
		}

		if (response.getContentType().find("application/json") == std::string::npos) {
			throw Poco::Exception{"Received content type is not of expected format (application/json)"};
		}

		content = std::string{std::istreambuf_iterator<char>{is},{}};
	}
	catch (const Poco::Exception& ex) {
		std::cout << "Request failed, reason: " << ex.displayText() << std::endl;
		throw ts_exception{"Request didn't yield expected content"};
	}

	return content;
}

} // namespace ts