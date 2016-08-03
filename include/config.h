#pragma once

#include <string>

namespace ts {

const std::string version_major = "0";
const std::string version_minor = "1";
const std::string version_patch = "0";

std::string get_version() {
	return version_major + "." + version_minor + "." + version_patch;
}

} // namespace ts