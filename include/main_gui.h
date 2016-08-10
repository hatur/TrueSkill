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

	void clear_raid_layout();

	void build_wait_layout();
	void remove_wait_layout();

	void push_wait_message(sf::String message);

	std::shared_ptr<tgui::Theme> m_tgui_theme {nullptr};
	std::vector<realm> m_cached_realmlist;
};

} // namespace ts