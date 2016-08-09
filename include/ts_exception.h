#pragma once

#include <exception>

namespace ts {

class ts_exception : public std::exception
{
public:
	ts_exception() {}
	ts_exception(const char* message)
		: std::exception{message} {}
};

} // namespace ts