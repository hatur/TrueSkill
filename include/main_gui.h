#pragma once

#include "gui.h"

namespace ts {

class ts_central;

class main_gui : public gui
{
public:
	main_gui(ts_central* central);

private:
	void build() override;

	void push_realm_list(std::vector<realm>&& realmlist);

	std::vector<realm> m_cached_realmlist;
};

} // namespace ts