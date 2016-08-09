#pragma once

#include <string>

#include <Poco/Net/Context.h>

namespace ts {
namespace config {

enum class log_level {
	error,
	warning,
	verbose,
	very_verbose
};

const std::string version_major = "0";
const std::string version_minor = "1";
const std::string version_patch = "0";

const Poco::Net::Context::Ptr sslContext{new Poco::Net::Context(Poco::Net::Context::CLIENT_USE, "", "", "",
											Poco::Net::Context::VERIFY_NONE, 9, false, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH")};

inline std::string get_version() {
	return version_major + "." + version_minor + "." + version_patch;
}

} // namespace config
} // namespace ts