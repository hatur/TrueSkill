#include "armory_client.h"

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

std::vector<realm> armory_client::retrieve_realm_list(std::string region) {
	using namespace Poco;
	using namespace Poco::Net;
	using namespace Poco::JSON;

	URI uri;
	uri.setScheme("https");
	uri.setAuthority(region + ".api.battle.net");
	uri.setPath("/wow/realm/status");
	uri.addQueryParameter("locale", "en_US");
	uri.addQueryParameter("apikey", "ejzptrf2wu4ud2eajhh44v3nsym23rrv");

	bool request_success = false;

	std::string content;

	content = send_mashery_request(uri);

	try {
		Parser parser;
		auto result = parser.parse(content);

		auto json_object = result.extract<Object::Ptr>();
		
		std::vector<realm> realmlist;

		auto realmArr = json_object->get("realms").extract<Array::Ptr>();
		
		for (const auto& realmEntry : *realmArr) {
			auto entryObject = realmEntry.extract<Object::Ptr>();
			auto country = entryObject->get("locale").extract<std::string>();
			country = country.substr(country.find("_") + 1);
			auto name = entryObject->get("name").extract<std::string>();

			//std::cout << "Found [" << country << "] " << name << std::endl;

			std::wstring utf8_country;
			std::wstring utf8_name;

			UnicodeConverter::toUTF8(country, utf8_country);
			UnicodeConverter::toUTF8(name, utf8_name);

			realmlist.emplace_back(utf8_country, utf8_name);
		}

		return realmlist;
	}
	catch (const Poco::Exception& ex) {
		auto exceptiontext = std::string{"retrieve_realm_list(): Error parsing json data, reason: "} + ex.displayText();
		throw ts_exception{exceptiontext.c_str()};
	}

	// test
	return std::vector<realm>{};
}

void armory_client::retrieve_raid_data(sf::String region, sf::String server, sf::String name, raid_data& io_raiddata_copy) {
	using namespace Poco;
	using namespace Poco::JSON;

	auto utf8_name = std::string{};
	auto utf8_server = std::string{};

	Poco::UnicodeConverter::toUTF8(server.toWideString(), utf8_server);
	Poco::UnicodeConverter::toUTF8(name.toWideString(), utf8_name);
	
	URI uri;
	uri.setScheme("https");
	uri.setAuthority(region + ".api.battle.net");
	uri.setPath("/wow/character/" + utf8_server + "/" + utf8_name);
	uri.addQueryParameter("fields", "achievements, progression");
	uri.addQueryParameter("locale", "en_US");
	uri.addQueryParameter("apikey", "ejzptrf2wu4ud2eajhh44v3nsym23rrv");

	const auto content = send_mashery_request(uri);

	//std::wstring utf8_content;

	//UnicodeConverter::toUTF8(content, utf8_content);

	//std::locale loc{std::locale::classic(), new std::codecvt_utf8<wchar_t>};

	//std::wofstream out{"testOut.txt", std::ios::out | std::ios::trunc};
	//out.imbue(loc);
	//out << utf8_content;

	try {
		Parser parser;
		auto result = parser.parse(content);

		auto json_object = result.extract<Object::Ptr>();

		auto achievements_data = json_object->get("achievements").extract<Object::Ptr>();
		auto achievements_completed = achievements_data->get("achievementsCompleted").extract<Array::Ptr>();

		std::cout << "Found " << achievements_completed->size() << " achievements!" << std::endl;

		for (auto& raid : *io_raiddata_copy.get_raids()) {
			for (auto& achievement : raid.m_armory_achievements) {
				auto it = std::find(achievements_completed->begin(), achievements_completed->end(), achievement.m_achievement_id);

				if (it == achievements_completed->end()) {
					achievement.m_unlocked = false;
				}
				else {
					achievement.m_unlocked = true;
				}

				std::cout << "Achievement " << achievement.m_achievement_name.toAnsiString() << " (ID: " << achievement.m_achievement_id << ") completed: " << achievement.m_unlocked << std::endl;
			}
		}

		auto progression_data = json_object->get("progression").extract<Object::Ptr>();
		auto progression_raids_data = progression_data->get("raids").extract<Array::Ptr>();

		for (auto& raid : *io_raiddata_copy.get_raids()) {
			auto it = std::find_if(progression_raids_data->begin(), progression_raids_data->end(), [raid] (const Dynamic::Var& var) {
				auto raid_obj = var.extract<Object::Ptr>();
				return raid_obj->get("name").extract<std::string>() == raid.m_raid_name;
			});

			if (it == progression_raids_data->end()) {
				throw ts_exception("armory_client::retrieve_raid_data(): couldn't find matching raid from progression data in the wow api");
			}

			auto raid_obj = it->extract<Object::Ptr>();
			auto boss_arr = raid_obj->get("bosses").extract<Array::Ptr>();

			std::cout << "Raid has " << boss_arr->size() << " bosses" << std::endl;

			for (auto& statistics : raid.m_armory_boss_statistics) {
				std::cout << "Searching for " << statistics.m_boss_name.toAnsiString() << std::endl;
				auto it2 = std::find_if(boss_arr->begin(), boss_arr->end(), [&statistics] (const Dynamic::Var& var) {
					return var.extract<Object::Ptr>()->get("name").extract<std::string>() == statistics.m_boss_name;
				});

				if (it2 == boss_arr->end()) {
					throw ts_exception("armory_client::retrieve_raid_data(): couldn't find matching raid from progression data in the wow api");
				}

				auto boss_obj = it2->extract<Object::Ptr>();

				// This is too static and may only work for raids that have fixed nhc/hc/m modes
				statistics.m_num_nhc_kills = static_cast<unsigned int>(boss_obj->get("normalKills").extract<int32_t>());
				statistics.m_num_hc_kills = static_cast<unsigned int>(boss_obj->get("heroicKills").extract<int32_t>());
				statistics.m_num_m_kills = static_cast<unsigned int>(boss_obj->get("mythicKills").extract<int32_t>());

				//std::cout << "Boss " << boss_obj->get("name").extract<std::string>() << " has " << statistics.m_num_nhc_kills << " nhc kills, " << statistics.m_num_hc_kills << " hc kills and "
				//	<< statistics.m_num_m_kills << " m kills." << std::endl;
			}
		}
	}
	catch (const Poco::Exception& ex) {
		throw ts_exception(ex.displayText().c_str());
	}
}

std::string armory_client::send_mashery_request(const Poco::URI& uri) {
	using namespace Poco::Net;

	auto path = uri.getPathAndQuery();
	if (path.empty()) {
		path = "/";
	}

	HTTPSClientSession session{uri.getHost(), uri.getPort(), ts::config::sslContext};
	session.setTimeout(Poco::Timespan{10, 0});

	bool request_success = false;
	std::string content;

	try {
		HTTPRequest request{HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1};
		request.setContentType("application/json;charset=UTF-8");
		session.sendRequest(request);

		HTTPResponse response;
		auto& is = session.receiveResponse(response);

		std::cout << "Status of response: " << response.getReason() << std::endl;
		std::cout << "Full response: " << std::endl;
		response.write(std::cout);
		std::cout << std::endl;

		if (response.getStatus() != HTTPResponse::HTTP_OK) {
			throw Poco::Exception{"Response didn't yield HTTP_OK"};
		}

		if (response.getContentType().find("application/json") == std::string::npos) {
			throw Poco::Exception{"Received content type is not of expected format (application/json)"};
		}

		content = std::string{std::istreambuf_iterator<char>{is},{}};

		request_success = true;
	}
	catch (const Poco::Exception& ex) {
		std::cout << "Request failed, reason: " << ex.what() << std::endl;
	}

	if (!request_success) {
		throw ts_exception{"Request didn't yield expected content"};
	}

	return content;
}

} // namepsace ts