#include "ts_central.h"
#include "main_gui.h"

#include <memory>

#include "conc_lock.h"
#include "style_theme.h"

namespace ts {

main_gui::main_gui(ts_central* ts_central)
	: gui{ts_central} {

}

void main_gui::log_message(sf::String message) {
	auto lock = conc_lock<gui>(this);
	m_log_box->addLine(std::move(message));
}

void main_gui::build() {
	auto lock = conc_lock<gui>(this);

	m_tgui_theme = std::make_shared<tgui::Theme>("data/theme/default.tg");

	tgui::ToolTip::setTimeToDisplay(sf::seconds(2));
	tgui::ToolTip::setDistanceToMouse({10.f, 10.f});

	tgui::Panel::Ptr search_panel = m_tgui_theme->load("SearchPanel");
	search_panel->setPosition(style_theme::main_layout_padding, style_theme::main_layout_padding);
	search_panel->setSize(sf::Vector2f{get_central()->get_renderwindow()->getSize()}.x - style_theme::main_layout_padding * 2, 112 - style_theme::main_layout_padding * 2);

	get_gui().add(search_panel);

	auto search_edit = std::make_shared<tgui::EditBox>();
	search_edit->setText("Character name");

	auto region_combo = std::make_shared<tgui::ComboBox>();
	region_combo->addItem("EU");
	region_combo->addItem("US");
	region_combo->setSelectedItemByIndex(0);
	region_combo->setItemsToDisplay(2);
	region_combo->setSize(tgui::Layout2d{sf::Vector2f{40, 18}});

	search_panel->add(region_combo);

	auto server_search_field = std::make_shared<tgui::EditBox>();
	auto server_suggestion_box = std::make_shared<tgui::ListBox>(); // pre defined
	server_search_field->setSize(160, 18);
	server_search_field->setPosition(44, 0);
	server_search_field->setText("retrieving list..");
	server_search_field->connect("TextChanged", [this, server_search_field, server_suggestion_box] {
		auto lock = conc_lock<gui>(this);

		if (m_cached_realmlist.size() == 0) {
			return;
		}

		server_suggestion_box->removeAllItems();

		std::vector<realm> matching_entries{};

		for (const auto& entry : m_cached_realmlist) {
			auto searchname = server_search_field->getText().toWideString();

			auto lowername = entry.m_name;
			auto lowersearch = searchname;

			std::transform(lowername.begin(), lowername.end(), lowername.begin(), ::tolower);
			std::transform(lowersearch.begin(), lowersearch.end(), lowersearch.begin(), ::tolower);

			auto pos = lowername.find(lowersearch);

			if (pos != std::wstring::npos) {
				matching_entries.push_back(entry);
			}
		}

		unsigned int counter = 0;
		for (const auto& entry : matching_entries) {
			server_suggestion_box->addItem(entry.m_name);

			++counter;

			if (counter >= server_suggestion_box->getMaximumItems()) {
				break;
			}
		}
	});

	search_panel->add(server_search_field);

	server_suggestion_box->setPosition(44, 22);
	server_suggestion_box->setItemHeight(18);
	server_suggestion_box->setMaximumItems(4);
	server_suggestion_box->setSize(160, 18 * server_suggestion_box->getMaximumItems());
	server_suggestion_box->connect("ItemSelected", [this, server_search_field, server_suggestion_box] {
		auto lock = conc_lock<gui>(this);
		server_search_field->setText(server_suggestion_box->getSelectedItem());

		auto cachedItem = server_suggestion_box->getSelectedItem();
		
		server_suggestion_box->removeAllItems();
		server_suggestion_box->addItem(cachedItem);
	});

	search_panel->add(server_suggestion_box);

	auto job = get_central()->get_threadpool()->job([this, region_combo, server_search_field, server_suggestion_box] {
		std::vector<realm> realmlist;

		try {
			realmlist = armory_client::retrieve_realm_list(region_combo->getSelectedItem());
		}
		catch (ts_exception& ex) {
			std::cout << ex.what() << std::endl;
			return;
		}

		{
			auto lock = conc_lock<gui>(this);
			server_search_field->setText("");

			if (realmlist.size() > 0) {
				for (int i = 0; i < server_suggestion_box->getMaximumItems(); ++i) {
					server_suggestion_box->addItem(realmlist[i].m_name);
				}
			}
		}

		push_realm_list(std::move(realmlist));
	});

	auto name_edit = std::make_shared<tgui::EditBox>();
	name_edit->setPosition(server_search_field->getPosition().x + server_search_field->getSize().x + 4, server_search_field->getPosition().y);
	name_edit->setSize(160, 18);

	search_panel->add(name_edit);

	// Raid Checkboxes

	raid_data raiddata{};

	float longest_total_width{0.f};
	unsigned int num_raid = 0;
	std::vector<std::pair<unsigned int, tgui::CheckBox::Ptr>> wcl_raids;

	for (const auto& raid : *raiddata.get_raids()) {
		tgui::CheckBox::Ptr raid_checkbox = m_tgui_theme->load("RoleSelection");
		raid_checkbox->setSize(18, 18);
		raid_checkbox->setPosition(name_edit->getPosition().x + name_edit->getSize().x + 4, name_edit->getPosition().y + raid_checkbox->getSize().y * num_raid);

		if (raid.m_default_raid) {
			raid_checkbox->check();
		}

		search_panel->add(raid_checkbox);

		tgui::Label::Ptr raid_checkbox_label = m_tgui_theme->load("RoleSelectionText");
		raid_checkbox_label->setText(raid.m_raid_name);
		raid_checkbox_label->setPosition(raid_checkbox->getPosition().x + raid_checkbox->getSize().x + 4, raid_checkbox->getPosition().y - 2);
	

		search_panel->add(raid_checkbox_label);

		auto total_width = raid_checkbox->getSize().x + raid_checkbox_label->getSize().x + 4;

		if (total_width > longest_total_width) {
			longest_total_width = total_width;
		}

		wcl_raids.push_back(std::make_pair(raid.m_wcl_zone_id, raid_checkbox));
		
		++num_raid;

		std::cout << raid.m_raid_name.toAnsiString() << std::endl;
	}

	std::cout << "longest width: " << longest_total_width << std::endl;

	// DPS Checkbox

	tgui::CheckBox::Ptr dps_checkbox = m_tgui_theme->load("RoleSelection");
	dps_checkbox->setSize(18, 18);
	dps_checkbox->setPosition(name_edit->getPosition().x + name_edit->getSize().x + longest_total_width + 8, name_edit->getPosition().y);
	dps_checkbox->check();

	search_panel->add(dps_checkbox);

	tgui::Label::Ptr dps_checkbox_text = m_tgui_theme->load("RoleSelectionText");
	dps_checkbox_text->setText("DPS");
	dps_checkbox_text->setPosition(dps_checkbox->getPosition().x + dps_checkbox->getSize().x + 4, dps_checkbox->getPosition().y - 2);

	search_panel->add(dps_checkbox_text);

	// HPS Checkbox

	tgui::CheckBox::Ptr hps_checkbox = m_tgui_theme->load("RoleSelection");
	hps_checkbox->setSize(18, 18);
	hps_checkbox->setPosition(dps_checkbox->getPosition().x, dps_checkbox->getPosition().y + dps_checkbox->getSize().y );

	search_panel->add(hps_checkbox);

	tgui::Label::Ptr hps_checkbox_text = m_tgui_theme->load("RoleSelectionText");
	hps_checkbox_text->setText("HPS");
	hps_checkbox_text->setPosition(hps_checkbox->getPosition().x + hps_checkbox->getSize().x + 4, hps_checkbox->getPosition().y);

	search_panel->add(hps_checkbox_text);

	// KRSI Checkbox

	tgui::CheckBox::Ptr krsi_checkbox = m_tgui_theme->load("RoleSelection");
	krsi_checkbox->setSize(18, 18);
	krsi_checkbox->setPosition(dps_checkbox->getPosition().x, hps_checkbox->getPosition().y + hps_checkbox->getSize().y);

	search_panel->add(krsi_checkbox);

	tgui::Label::Ptr krsi_checkbox_text = m_tgui_theme->load("RoleSelectionText");
	krsi_checkbox_text->setText("KRSI");
	krsi_checkbox_text->setPosition(krsi_checkbox->getPosition().x + krsi_checkbox->getSize().x + 4, krsi_checkbox->getPosition().y);

	search_panel->add(krsi_checkbox_text);

	tgui::Button::Ptr search_button = std::make_shared<tgui::Button>();
	search_button->setPosition(krsi_checkbox_text->getPosition().x + krsi_checkbox_text->getSize().x + 4, dps_checkbox_text->getPosition().y + 2);
	search_button->setSize(64, 18);
	search_button->setText("search");

	search_button->connect("Pressed", [this, region_combo, server_search_field, name_edit, wcl_raids, dps_checkbox, hps_checkbox, krsi_checkbox] {
		std::vector<metric> metrics;

		if (dps_checkbox->isChecked()) {
			metrics.push_back(metric::DPS);
		}
		if (hps_checkbox->isChecked()) {
			metrics.push_back(metric::HPS);
		}
		if (krsi_checkbox->isChecked()) {
			metrics.push_back(metric::KRSI);
		}

		build_raid_layout(region_combo->getSelectedItem(), server_search_field->getText(), name_edit->getText(), std::move(wcl_raids), std::move(metrics)); 
	});

	search_panel->add(search_button);

	tgui::Button::Ptr quick_search_button = std::make_shared<tgui::Button>();
	quick_search_button->setPosition(search_button->getPosition() + sf::Vector2f{search_button->getSize().x + 2.f, 0.f});
	quick_search_button->setSize(64, 18);
	quick_search_button->setText("quicksearch");

	quick_search_button->connect("Pressed", [this, region_combo, name_edit, search_button, wcl_raids, dps_checkbox, hps_checkbox, krsi_checkbox] {
		std::vector<metric> metrics;

		if (dps_checkbox->isChecked()) {
			metrics.push_back(metric::DPS);
		}
		if (hps_checkbox->isChecked()) {
			metrics.push_back(metric::HPS);
		}
		if (krsi_checkbox->isChecked()) {
			metrics.push_back(metric::KRSI);
		}

		build_raid_layout("EU", "Nethersturm", "Hatura", std::move(wcl_raids), std::move(metrics));
	});

	search_panel->add(quick_search_button);

	const float log_box_size = 500.f;

	m_log_box = std::make_shared<tgui::ChatBox>();
	m_log_box->setPosition(search_panel->getSize().x - log_box_size, quick_search_button->getPosition().y);
	m_log_box->setSize(log_box_size, 94);
	m_log_box->setTextSize(12);
	m_log_box->setNewLinesBelowOthers(false);

	search_panel->add(m_log_box);
}

void main_gui::push_realm_list(std::vector<realm>&& realmlist) {
	auto lock = conc_lock<gui>(this);
	m_cached_realmlist = std::move(realmlist);
}

void main_gui::build_raid_layout(sf::String region, sf::String server, sf::String name, std::vector<std::pair<unsigned int, tgui::CheckBox::Ptr>> wcl_raids, std::vector<metric> wcl_metrics) {
	{
		if (m_request_ongoing) {
			std::cout << "search request ongoing.." << std::endl;
			return;
		}
		else {
			m_request_ongoing = true;
		}
	}

	// final act is a shread_ptr and caught in the thread lambda to ensure scoped-exit execution
	const auto act_function = [this] { m_request_ongoing = false; };
	const auto act = std::make_shared<gsl::final_act<decltype(act_function)>>(std::move(act_function));

	clear_raid_layout();

	// Maybe add waiting layout here

	remove_wait_layout();
	build_wait_layout();

	auto log_info_string = sf::String{"Request started for "};
	log_info_string += name;
	log_info_string += " on server ";
	log_info_string += server;
	log_info_string += " (";
	log_info_string += region;
	log_info_string += ")";

	log_message(std::move(log_info_string));

	auto result = get_central()->get_threadpool()->job([this, region(std::move(region)), server(std::move(server)), name(std::move(name)), wcl_raids(std::move(wcl_raids)), wcl_metrics(std::move(wcl_metrics)), act] {
		auto local_raiddata = std::make_unique<raid_data>();
		if (!local_raiddata->is_initialized()) {
			std::cout << "main_gui::build_raid_layout(): local raiddata not properly initialized!" << std::endl;
			return;
		}

		push_wait_message(">> retrieving minimum data..");

		// We acquire data from 2 sources async, we need copies here just to be safe..

		auto armory_raiddata_copy = *local_raiddata.get();
		auto warcraftlogs_raiddata_rankings_copy = *local_raiddata.get();
		auto warcraftlogs_raiddata_parse_copy = *local_raiddata.get();

		auto armory_job = get_central()->get_threadpool()->job([this, region, server, name, &armory_raiddata_copy] {
			try {
				log_message("Starting job: Armory data");
				armory_client::retrieve_raid_data(region, server, name, armory_raiddata_copy);
				log_message("Finished job: Armory data");
			}
			catch (const ts_exception& ex) {
				auto error_string = sf::String{"Error for job: couldn't retrieve armory data, reason: "} + sf::String{ex.what()};
				std::cout << error_string.toAnsiString() << std::endl;
				log_message(std::move(error_string));
			}
		});

		std::vector<unsigned int> wcl_selected_raids;
		for (const auto& raid : wcl_raids) {
			if (raid.second->isChecked()) {
				wcl_selected_raids.push_back(raid.first);
			}
		}

		auto warcraftlogs_top_job = get_central()->get_threadpool()->job([this, region, server, name, &warcraftlogs_raiddata_rankings_copy, wcl_selected_raids, wcl_metrics] {
			try {
				log_message("Starting job: Warcraftlogs ranking data..");
				warcraftlogs_client::retrieve_ranking_log_data(region, server, name, wcl_selected_raids, wcl_metrics, *get_central()->get_threadpool(), warcraftlogs_raiddata_rankings_copy);
				log_message("Finished job: Warcraftlogs ranking data..");
			}
			catch (const ts_exception& ex) {
				auto error_string = sf::String{"Error for job: couldn't retrieve Warcraftlogs ranking data, reason: "} + sf::String{ex.what()};
				std::cout << error_string.toAnsiString() << std::endl;
				log_message(std::move(error_string));
			}
		});

		auto warcraftlogs_median_job = get_central()->get_threadpool()->job([this, region, server, name, &warcraftlogs_raiddata_parse_copy, wcl_selected_raids, wcl_metrics] {
			try {
				log_message("Starting job: Detailled warcraftlogs parse data..");
				warcraftlogs_client::retrieve_parse_log_data(region, server, name, wcl_selected_raids, wcl_metrics, *get_central()->get_threadpool(), warcraftlogs_raiddata_parse_copy);
				log_message("Finished job: Detailled warcraftlogs parse data..");
			}
			catch (const ts_exception& ex) {
				auto error_string = sf::String{"Error for job: couldn't retrieve detailled warcraftlogs data, reason: "} + sf::String{ex.what()};
				std::cout << error_string.toAnsiString() << std::endl;
				log_message(std::move(error_string));
			}
		});

		// We finish the armory job the get the minimum needed data..

		armory_job.get();

		// Fill the data

		for (auto& raid : *local_raiddata->get_raids()) {
			raid.m_armory_achievements = std::move(armory_raiddata_copy.get_raid(raid.m_raid_name)->m_armory_achievements);
			raid.m_armory_boss_statistics = std::move(armory_raiddata_copy.get_raid(raid.m_raid_name)->m_armory_boss_statistics);
		}

		{
			auto lock = conc_lock<gui>(this);

			tgui::Panel::Ptr raid_main_layout = m_tgui_theme->load("RaidOuter");
			raid_main_layout->setPosition(style_theme::main_layout_padding, 112);
			raid_main_layout->setSize(get_central()->get_renderwindow()->getSize().x - style_theme::main_layout_padding * 2
				, get_gui().getWindow()->getSize().y - style_theme::main_layout_padding - raid_main_layout->getPosition().y);

			int num_x = 0;
			for (const auto& raid : *local_raiddata->get_raids()) {
				auto it = std::find(wcl_selected_raids.begin(), wcl_selected_raids.end(), raid.m_wcl_zone_id);

				if (it == wcl_selected_raids.end()) {
					continue;
				}

				const float panel_width = 200;
				const float raid_instance_panel_inside_padding = 2;
				const float raid_instance_panel_section_padding = 10;

				tgui::Panel::Ptr raid_instance_panel = m_tgui_theme->load("RaidInner");
				raid_main_layout->add(raid_instance_panel, raid.m_raid_name + "_panel");
				raid_instance_panel->setPosition(4 + num_x * panel_width + num_x * 4, 4);
				raid_instance_panel->setSize(panel_width, raid_instance_panel->getParent()->getSize().y - 8);

				auto raid_icon = std::make_shared<tgui::Picture>();
				raid_icon->setTexture(raid.m_icon_path);
				raid_icon->setPosition(raid_instance_panel_inside_padding, raid_instance_panel_inside_padding);
				raid_icon->setSize(48, 48);
				raid_icon->setSmooth(true);

				raid_instance_panel->add(raid_icon);

				tgui::Label::Ptr raid_name = m_tgui_theme->load("RaidHeading");
				raid_name->setText(raid.m_raid_name);
				raid_name->setPosition(raid_icon->getPosition().x + raid_icon->getSize().x + raid_instance_panel_inside_padding, 0);
				raid_name->setTextSize(16);

				raid_instance_panel->add(raid_name);

				tgui::Label::Ptr acc_progress_label = m_tgui_theme->load("RaidTypeHeading");
				acc_progress_label->setText("Account progression");
				acc_progress_label->setPosition(raid_icon->getPosition() + sf::Vector2f{0.f, raid_icon->getSize().y} +sf::Vector2f{0.f, raid_instance_panel_section_padding});
				acc_progress_label->setTextSize(14);

				raid_instance_panel->add(acc_progress_label);

				//tgui::Label::Ptr mythic_armory_progress_label = m_tgui_theme->load("RaidInfo");
				//mythic_armory_progress_label->setText("mythic-mode:");
				//mythic_armory_progress_label->setPosition(acc_progress_label->getPosition() + sf::Vector2f{0.f, static_cast<float>(acc_progress_label->getTextSize())} +sf::Vector2f{0.f, raid_instance_panel_section_padding * 1.25f});
				//mythic_armory_progress_label->setTextSize(10);

				//raid_instance_panel->add(mythic_armory_progress_label);

				auto mythic_armory_progress_bar = std::make_shared<tgui::ProgressBar>();
				mythic_armory_progress_bar->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
				mythic_armory_progress_bar->setMinimum(0);
				mythic_armory_progress_bar->setMaximum(100);
				mythic_armory_progress_bar->setSize(style_theme::progress_bar_width, 14);
				mythic_armory_progress_bar->setText("mythic");
				mythic_armory_progress_bar->setPosition(acc_progress_label->getPosition() + sf::Vector2f{0.f, static_cast<float>(acc_progress_label->getTextSize()) + 2.f} +sf::Vector2f{0.f, raid_instance_panel_inside_padding});

				const float mythic_ach_progression = static_cast<float>(raid.get_armory_num_killed_bosses_acc()) * 100.f / static_cast<float>(raid.get_total_bosses());

				mythic_armory_progress_bar->setValue(static_cast<unsigned int>(mythic_ach_progression));

				raid_instance_panel->add(mythic_armory_progress_bar, "mythic_armory_progress_bar");

				tgui::Panel::Ptr mythic_armory_progress_bar_tooltip = m_tgui_theme->load("ToolTip");

				int num_ach = 0;
				float widest_label = 0.f;
				for (const auto achievement : raid.m_armory_achievements) {
					if (achievement.m_achievement_type != armory_raid_achievement_type::mythic_boss) {
						continue;
					}

					const auto full_label_text = [&achievement] {
						if (achievement.m_unlocked) {
							return sf::String{"[killed] " + achievement.m_achievement_name};
						}
						else {
							return sf::String{"[open] " + achievement.m_achievement_name};
						}
					}();

					tgui::Label::Ptr label = [this, &achievement] {
						if (achievement.m_unlocked) {
							return m_tgui_theme->load("ToolTip_Killed");
						}
						else {
							return m_tgui_theme->load("ToolTip_Open");
						}
					}();

					label->setTextSize(style_theme::tooltip_text_size);
					label->setText(full_label_text);
					label->setPosition(2, num_ach * style_theme::tooltip_text_size + 2);

					const auto label_width = label->getSize().x;
					if (label_width > widest_label) {
						widest_label = label_width;
					}

					mythic_armory_progress_bar_tooltip->add(label);

					++num_ach;
				}

				mythic_armory_progress_bar_tooltip->setSize(widest_label * 2.f, num_ach * style_theme::tooltip_text_size + 5.f);

				auto mythic_armory_progress_bar_tooltip_wrapper = std::make_unique<tooltip_wrapper>(&get_gui(), mythic_armory_progress_bar, mythic_armory_progress_bar_tooltip);
				register_tooltip(std::move(mythic_armory_progress_bar_tooltip_wrapper));

				tgui::Label::Ptr char_progression_label = m_tgui_theme->load("RaidTypeHeading");
				char_progression_label->setText("Character progression");
				char_progression_label->setPosition(mythic_armory_progress_bar->getPosition() + sf::Vector2f{0.f, 14.f + raid_instance_panel_section_padding});
				char_progression_label->setTextSize(14);

				raid_instance_panel->add(char_progression_label);

				// nhc progress bar label

				//tgui::Label::Ptr nhc_progression_label = m_tgui_theme->load("RaidInfo");
				//nhc_progression_label->setText("normal-mode:");
				//nhc_progression_label->setPosition(char_progression_label->getPosition() + sf::Vector2f{0.f, static_cast<float>(char_progression_label->getTextSize())} +sf::Vector2f{0.f, raid_instance_panel_section_padding * 1.25f});
				//nhc_progression_label->setTextSize(10);

				//raid_instance_panel->add(nhc_progression_label);

				// nhc progress bar

				auto nhc_armory_progress_bar = std::make_shared<tgui::ProgressBar>();
				nhc_armory_progress_bar->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
				nhc_armory_progress_bar->setMinimum(0);
				nhc_armory_progress_bar->setMaximum(100);
				nhc_armory_progress_bar->setSize(style_theme::progress_bar_width, 14);
				nhc_armory_progress_bar->setText("normal");
				nhc_armory_progress_bar->setPosition(char_progression_label->getPosition() + sf::Vector2f{0.f, static_cast<float>(char_progression_label->getTextSize()) + raid_instance_panel_inside_padding * 2});

				const float nhc_char_progression = static_cast<float>(raid.get_armory_num_killed_bosses_char_nhc()) * 100.f / static_cast<float>(raid.get_total_bosses());

				nhc_armory_progress_bar->setValue(static_cast<unsigned int>(nhc_char_progression));

				// nhc progress bar tooltip

				tgui::Panel::Ptr nhc_armory_progress_tooltip = m_tgui_theme->load("ToolTip");
				fill_armory_character_statistics_tooltip(nhc_armory_progress_tooltip.get(), raid, boss_difficulty::normal);

				auto nhc_armory_progress_bar_tooltip_wrapper = std::make_unique<tooltip_wrapper>(&get_gui(), nhc_armory_progress_bar, nhc_armory_progress_tooltip);
				register_tooltip(std::move(nhc_armory_progress_bar_tooltip_wrapper));

				raid_instance_panel->add(nhc_armory_progress_bar, "nhc_armory_progress_bar");

				// hc progress label

				//tgui::Label::Ptr hc_progression_label = m_tgui_theme->load("RaidInfo");
				//hc_progression_label->setText("hardcore-mode:");
				//hc_progression_label->setPosition(nhc_armory_progress_bar->getPosition() + sf::Vector2f{0.f, nhc_armory_progress_bar->getSize().y} +sf::Vector2f{0.f, raid_instance_panel_section_padding * 1.25f});
				//hc_progression_label->setTextSize(10);

				//raid_instance_panel->add(hc_progression_label);

				// hc progress bar

				auto hc_armory_progress_bar = std::make_shared<tgui::ProgressBar>();
				hc_armory_progress_bar->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
				hc_armory_progress_bar->setMinimum(0);
				hc_armory_progress_bar->setMaximum(100);
				hc_armory_progress_bar->setSize(style_theme::progress_bar_width, 14);
				hc_armory_progress_bar->setText("heroic");
				hc_armory_progress_bar->setPosition(nhc_armory_progress_bar->getPosition() + sf::Vector2f{0.f, static_cast<float>(nhc_armory_progress_bar->getSize().y) + raid_instance_panel_inside_padding * 2});

				const float hc_char_progression = static_cast<float>(raid.get_armory_num_killed_bosses_char_hc()) * 100.f / static_cast<float>(raid.get_total_bosses());

				hc_armory_progress_bar->setValue(static_cast<unsigned int>(hc_char_progression));

				// hc progress bar tooltip

				tgui::Panel::Ptr hc_armory_progress_tooltip = m_tgui_theme->load("ToolTip");
				fill_armory_character_statistics_tooltip(hc_armory_progress_tooltip.get(), raid, boss_difficulty::heroic);

				auto hc_armory_progress_bar_tooltip_wrapper = std::make_unique<tooltip_wrapper>(&get_gui(), hc_armory_progress_bar, hc_armory_progress_tooltip);
				register_tooltip(std::move(hc_armory_progress_bar_tooltip_wrapper));

				raid_instance_panel->add(hc_armory_progress_bar, "hc_armory_progress_bar");

				// m progress label

				//tgui::Label::Ptr m_progression_label = m_tgui_theme->load("RaidInfo");
				//m_progression_label->setText("mythic-mode:");
				//m_progression_label->setPosition(hc_armory_progress_bar->getPosition() + sf::Vector2f{0.f, hc_armory_progress_bar->getSize().y} +sf::Vector2f{0.f, raid_instance_panel_section_padding * 1.25f});
				//m_progression_label->setTextSize(10);

				//raid_instance_panel->add(m_progression_label);

				// m progress bar

				auto m_armory_progress_bar = std::make_shared<tgui::ProgressBar>();
				m_armory_progress_bar->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
				m_armory_progress_bar->setMinimum(0);
				m_armory_progress_bar->setMaximum(100);
				m_armory_progress_bar->setSize(style_theme::progress_bar_width, 14);
				m_armory_progress_bar->setText("mythic");
				m_armory_progress_bar->setPosition(hc_armory_progress_bar->getPosition() + sf::Vector2f{0.f, static_cast<float>(hc_armory_progress_bar->getSize().y) + raid_instance_panel_inside_padding * 2});

				const float m_char_progression = static_cast<float>(raid.get_armory_num_killed_bosses_char_m()) * 100.f / static_cast<float>(raid.get_total_bosses());

				m_armory_progress_bar->setValue(static_cast<unsigned int>(m_char_progression));

				// m progress bar tolotip

				tgui::Panel::Ptr m_armory_progress_tooltip = m_tgui_theme->load("ToolTip");
				fill_armory_character_statistics_tooltip(m_armory_progress_tooltip.get(), raid, boss_difficulty::mythic);

				auto m_armory_progress_bar_tooltip_wrapper = std::make_unique<tooltip_wrapper>(&get_gui(), m_armory_progress_bar, m_armory_progress_tooltip);
				register_tooltip(std::move(m_armory_progress_bar_tooltip_wrapper));

				raid_instance_panel->add(m_armory_progress_bar, "m_armory_progress_bar");

				if (wcl_metrics.size() > 0) {
					const auto dps_m_it = std::find(wcl_metrics.cbegin(), wcl_metrics.cend(), metric::DPS);

					auto anchor_position = m_armory_progress_bar->getPosition() + sf::Vector2f{0.f, static_cast<float>(m_armory_progress_bar->getSize().y + raid_instance_panel_section_padding)};

					if (dps_m_it != wcl_metrics.cend()) {
						tgui::Label::Ptr warcraftlogs_rankings_label = m_tgui_theme->load("RaidTypeHeading");
						warcraftlogs_rankings_label->setText("WCL DPS Rankings");
						warcraftlogs_rankings_label->setPosition(anchor_position);
						warcraftlogs_rankings_label->setTextSize(14);

						raid_instance_panel->add(warcraftlogs_rankings_label);

						// HC Top Bars

						auto dps_bracket_bar_hc = std::make_shared<tgui::ProgressBar>();
						dps_bracket_bar_hc->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						dps_bracket_bar_hc->setMinimum(0);
						dps_bracket_bar_hc->setMaximum(100);
						dps_bracket_bar_hc->setSize(style_theme::wcl_progress_bar_width, 14);
						dps_bracket_bar_hc->setText("loading information..");
						dps_bracket_bar_hc->setPosition(warcraftlogs_rankings_label->getPosition() + sf::Vector2f{0.f, warcraftlogs_rankings_label->getTextSize() + raid_instance_panel_inside_padding * 2});

						raid_instance_panel->add(dps_bracket_bar_hc, "dps_top_bracket_bar_hc");

						auto dps_overall_bar_hc = std::make_shared<tgui::ProgressBar>();
						dps_overall_bar_hc->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						dps_overall_bar_hc->setMinimum(0);
						dps_overall_bar_hc->setMaximum(100);
						dps_overall_bar_hc->setSize(style_theme::wcl_progress_bar_width, 14);
						dps_overall_bar_hc->setText("loading information..");
						dps_overall_bar_hc->setPosition(dps_bracket_bar_hc->getPosition() + sf::Vector2f{0.f, dps_bracket_bar_hc->getSize().y + raid_instance_panel_inside_padding * 2});

						raid_instance_panel->add(dps_overall_bar_hc, "dps_top_overall_bar_hc");

						// M Top Bars

						auto dps_bracket_bar_m = std::make_shared<tgui::ProgressBar>();
						dps_bracket_bar_m->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						dps_bracket_bar_m->setMinimum(0);
						dps_bracket_bar_m->setMaximum(100);
						dps_bracket_bar_m->setSize(style_theme::wcl_progress_bar_width, 14);
						dps_bracket_bar_m->setText("loading information..");
						dps_bracket_bar_m->setPosition(dps_bracket_bar_hc->getPosition() + sf::Vector2f{dps_bracket_bar_hc->getSize().x + style_theme::wcl_horizontal_padding, 0.f});

						raid_instance_panel->add(dps_bracket_bar_m, "dps_top_bracket_bar_m");

						auto dps_overall_bar_m = std::make_shared<tgui::ProgressBar>();
						dps_overall_bar_m->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						dps_overall_bar_m->setMinimum(0);
						dps_overall_bar_m->setMaximum(100);
						dps_overall_bar_m->setSize(style_theme::wcl_progress_bar_width, 14);
						dps_overall_bar_m->setText("loading information..");
						dps_overall_bar_m->setPosition(dps_overall_bar_hc->getPosition() + sf::Vector2f{dps_overall_bar_hc->getSize().x + style_theme::wcl_horizontal_padding, 0.f});

						raid_instance_panel->add(dps_overall_bar_m, "dps_top_overall_bar_m");

						// HC Median Bars

						auto dps_median_bracket_bar_hc = std::make_shared<tgui::ProgressBar>();
						dps_median_bracket_bar_hc->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						dps_median_bracket_bar_hc->setMinimum(0);
						dps_median_bracket_bar_hc->setMaximum(100);
						dps_median_bracket_bar_hc->setSize(style_theme::wcl_progress_bar_width, 14);
						dps_median_bracket_bar_hc->setText("tbi [median bracket]");
						dps_median_bracket_bar_hc->setPosition(dps_overall_bar_hc->getPosition() + sf::Vector2f{0.f, dps_overall_bar_hc->getSize().y + raid_instance_panel_inside_padding * 2});

						raid_instance_panel->add(dps_median_bracket_bar_hc, "dps_median_bracket_bar_hc");

						auto dps_median_overall_bar_hc = std::make_shared<tgui::ProgressBar>();
						dps_median_overall_bar_hc->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						dps_median_overall_bar_hc->setMinimum(0);
						dps_median_overall_bar_hc->setMaximum(100);
						dps_median_overall_bar_hc->setSize(style_theme::wcl_progress_bar_width, 14);
						dps_median_overall_bar_hc->setText("tbi [overall bracket]");
						dps_median_overall_bar_hc->setPosition(dps_median_bracket_bar_hc->getPosition() + sf::Vector2f{0.f, dps_median_bracket_bar_hc->getSize().y + raid_instance_panel_inside_padding * 2});

						raid_instance_panel->add(dps_median_overall_bar_hc, "dps_median_overall_bar_hc");

						// M Median Bars

						auto dps_median_bracket_bar_m = std::make_shared<tgui::ProgressBar>();
						dps_median_bracket_bar_m->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						dps_median_bracket_bar_m->setMinimum(0);
						dps_median_bracket_bar_m->setMaximum(100);
						dps_median_bracket_bar_m->setSize(style_theme::wcl_progress_bar_width, 14);
						dps_median_bracket_bar_m->setText("tbi [median bracket]");
						dps_median_bracket_bar_m->setPosition(dps_median_bracket_bar_hc->getPosition() + sf::Vector2f{dps_median_bracket_bar_hc->getSize().x + style_theme::wcl_horizontal_padding, 0.f});

						raid_instance_panel->add(dps_median_bracket_bar_m, "dps_median_bracket_bar_m");

						auto dps_median_overall_bar_m = std::make_shared<tgui::ProgressBar>();
						dps_median_overall_bar_m->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						dps_median_overall_bar_m->setMinimum(0);
						dps_median_overall_bar_m->setMaximum(100);
						dps_median_overall_bar_m->setSize(style_theme::wcl_progress_bar_width, 14);
						dps_median_overall_bar_m->setText("tbi [overall bracket]");
						dps_median_overall_bar_m->setPosition(dps_median_overall_bar_hc->getPosition() + sf::Vector2f{dps_median_overall_bar_hc->getSize().x + style_theme::wcl_horizontal_padding, 0.f});

						raid_instance_panel->add(dps_median_overall_bar_m, "dps_median_overall_bar_m");

						anchor_position = dps_median_overall_bar_hc->getPosition() + sf::Vector2f{0.f, dps_median_overall_bar_hc->getSize().y + raid_instance_panel_section_padding};
					}

					const auto hps_m_it = std::find(wcl_metrics.cbegin(), wcl_metrics.cend(), metric::HPS);

					if (hps_m_it != wcl_metrics.cend()) {
						tgui::Label::Ptr warcraftlogs_rankings_label = m_tgui_theme->load("RaidTypeHeading");
						warcraftlogs_rankings_label->setText("WCL HPS Rankings");
						warcraftlogs_rankings_label->setPosition(anchor_position);
						warcraftlogs_rankings_label->setTextSize(14);

						raid_instance_panel->add(warcraftlogs_rankings_label);

						// HC Top Bars

						auto hps_bracket_bar_hc = std::make_shared<tgui::ProgressBar>();
						hps_bracket_bar_hc->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						hps_bracket_bar_hc->setMinimum(0);
						hps_bracket_bar_hc->setMaximum(100);
						hps_bracket_bar_hc->setSize(style_theme::wcl_progress_bar_width, 14);
						hps_bracket_bar_hc->setText("loading information..");
						hps_bracket_bar_hc->setPosition(warcraftlogs_rankings_label->getPosition() + sf::Vector2f{0.f, warcraftlogs_rankings_label->getTextSize() + raid_instance_panel_inside_padding * 2});

						raid_instance_panel->add(hps_bracket_bar_hc, "hps_top_bracket_bar_hc");

						auto hps_overall_bar_hc = std::make_shared<tgui::ProgressBar>();
						hps_overall_bar_hc->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						hps_overall_bar_hc->setMinimum(0);
						hps_overall_bar_hc->setMaximum(100);
						hps_overall_bar_hc->setSize(style_theme::wcl_progress_bar_width, 14);
						hps_overall_bar_hc->setText("loading information..");
						hps_overall_bar_hc->setPosition(hps_bracket_bar_hc->getPosition() + sf::Vector2f{0.f, hps_bracket_bar_hc->getSize().y + raid_instance_panel_inside_padding * 2});

						raid_instance_panel->add(hps_overall_bar_hc, "hps_top_overall_bar_hc");

						// M Top Bars

						auto hps_bracket_bar_m = std::make_shared<tgui::ProgressBar>();
						hps_bracket_bar_m->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						hps_bracket_bar_m->setMinimum(0);
						hps_bracket_bar_m->setMaximum(100);
						hps_bracket_bar_m->setSize(style_theme::wcl_progress_bar_width, 14);
						hps_bracket_bar_m->setText("loading information..");
						hps_bracket_bar_m->setPosition(hps_bracket_bar_hc->getPosition() + sf::Vector2f{hps_bracket_bar_hc->getSize().x + style_theme::wcl_horizontal_padding, 0.f});

						raid_instance_panel->add(hps_bracket_bar_m, "hps_top_bracket_bar_m");

						auto hps_overall_bar_m = std::make_shared<tgui::ProgressBar>();
						hps_overall_bar_m->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						hps_overall_bar_m->setMinimum(0);
						hps_overall_bar_m->setMaximum(100);
						hps_overall_bar_m->setSize(style_theme::wcl_progress_bar_width, 14);
						hps_overall_bar_m->setText("loading information..");
						hps_overall_bar_m->setPosition(hps_overall_bar_hc->getPosition() + sf::Vector2f{hps_overall_bar_hc->getSize().x + style_theme::wcl_horizontal_padding, 0.f});

						raid_instance_panel->add(hps_overall_bar_m, "hps_top_overall_bar_m");

						// HC Median Bars

						auto hps_median_bracket_bar_hc = std::make_shared<tgui::ProgressBar>();
						hps_median_bracket_bar_hc->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						hps_median_bracket_bar_hc->setMinimum(0);
						hps_median_bracket_bar_hc->setMaximum(100);
						hps_median_bracket_bar_hc->setSize(style_theme::wcl_progress_bar_width, 14);
						hps_median_bracket_bar_hc->setText("tbi [median bracket]");
						hps_median_bracket_bar_hc->setPosition(hps_overall_bar_hc->getPosition() + sf::Vector2f{0.f, hps_overall_bar_hc->getSize().y + raid_instance_panel_inside_padding * 2});

						raid_instance_panel->add(hps_median_bracket_bar_hc, "hps_median_bracket_bar_hc");

						auto hps_median_overall_bar_hc = std::make_shared<tgui::ProgressBar>();
						hps_median_overall_bar_hc->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						hps_median_overall_bar_hc->setMinimum(0);
						hps_median_overall_bar_hc->setMaximum(100);
						hps_median_overall_bar_hc->setSize(style_theme::wcl_progress_bar_width, 14);
						hps_median_overall_bar_hc->setText("tbi [overall bracket]");
						hps_median_overall_bar_hc->setPosition(hps_median_bracket_bar_hc->getPosition() + sf::Vector2f{0.f, hps_median_bracket_bar_hc->getSize().y + raid_instance_panel_inside_padding * 2});

						raid_instance_panel->add(hps_median_overall_bar_hc, "hps_median_overall_bar_hc");

						// M Median Bars

						auto hps_median_bracket_bar_m = std::make_shared<tgui::ProgressBar>();
						hps_median_bracket_bar_m->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						hps_median_bracket_bar_m->setMinimum(0);
						hps_median_bracket_bar_m->setMaximum(100);
						hps_median_bracket_bar_m->setSize(style_theme::wcl_progress_bar_width, 14);
						hps_median_bracket_bar_m->setText("tbi [median bracket]");
						hps_median_bracket_bar_m->setPosition(hps_median_bracket_bar_hc->getPosition() + sf::Vector2f{hps_median_bracket_bar_hc->getSize().x + style_theme::wcl_horizontal_padding, 0.f});

						raid_instance_panel->add(hps_median_bracket_bar_m, "hps_median_bracket_bar_m");

						auto hps_median_overall_bar_m = std::make_shared<tgui::ProgressBar>();
						hps_median_overall_bar_m->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						hps_median_overall_bar_m->setMinimum(0);
						hps_median_overall_bar_m->setMaximum(100);
						hps_median_overall_bar_m->setSize(style_theme::wcl_progress_bar_width, 14);
						hps_median_overall_bar_m->setText("tbi [overall bracket]");
						hps_median_overall_bar_m->setPosition(hps_median_overall_bar_hc->getPosition() + sf::Vector2f{hps_median_overall_bar_hc->getSize().x + style_theme::wcl_horizontal_padding, 0.f});

						raid_instance_panel->add(hps_median_overall_bar_m, "hps_median_overall_bar_m");

						anchor_position = hps_median_overall_bar_hc->getPosition() + sf::Vector2f{0.f, hps_median_overall_bar_hc->getSize().y + raid_instance_panel_section_padding};
					}

					const auto krsi_m_it = std::find(wcl_metrics.cbegin(), wcl_metrics.cend(), metric::KRSI);

					if (krsi_m_it != wcl_metrics.cend()) {
						tgui::Label::Ptr warcraftlogs_rankings_label = m_tgui_theme->load("RaidTypeHeading");
						warcraftlogs_rankings_label->setText("WCL KRSI Rankings");
						warcraftlogs_rankings_label->setPosition(anchor_position);
						warcraftlogs_rankings_label->setTextSize(14);

						raid_instance_panel->add(warcraftlogs_rankings_label);

						// HC Top Bars

						auto krsi_bracket_bar_hc = std::make_shared<tgui::ProgressBar>();
						krsi_bracket_bar_hc->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						krsi_bracket_bar_hc->setMinimum(0);
						krsi_bracket_bar_hc->setMaximum(100);
						krsi_bracket_bar_hc->setSize(style_theme::wcl_progress_bar_width, 14);
						krsi_bracket_bar_hc->setText("loading information..");
						krsi_bracket_bar_hc->setPosition(warcraftlogs_rankings_label->getPosition() + sf::Vector2f{0.f, warcraftlogs_rankings_label->getTextSize() + raid_instance_panel_inside_padding * 2});

						raid_instance_panel->add(krsi_bracket_bar_hc, "krsi_top_bracket_bar_hc");

						auto krsi_overall_bar_hc = std::make_shared<tgui::ProgressBar>();
						krsi_overall_bar_hc->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						krsi_overall_bar_hc->setMinimum(0);
						krsi_overall_bar_hc->setMaximum(100);
						krsi_overall_bar_hc->setSize(style_theme::wcl_progress_bar_width, 14);
						krsi_overall_bar_hc->setText("loading information..");
						krsi_overall_bar_hc->setPosition(krsi_bracket_bar_hc->getPosition() + sf::Vector2f{0.f, krsi_bracket_bar_hc->getSize().y + raid_instance_panel_inside_padding * 2});

						raid_instance_panel->add(krsi_overall_bar_hc, "krsi_top_overall_bar_hc");

						// M Top Bars

						auto krsi_bracket_bar_m = std::make_shared<tgui::ProgressBar>();
						krsi_bracket_bar_m->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						krsi_bracket_bar_m->setMinimum(0);
						krsi_bracket_bar_m->setMaximum(100);
						krsi_bracket_bar_m->setSize(style_theme::wcl_progress_bar_width, 14);
						krsi_bracket_bar_m->setText("loading information..");
						krsi_bracket_bar_m->setPosition(krsi_bracket_bar_hc->getPosition() + sf::Vector2f{krsi_bracket_bar_hc->getSize().x + style_theme::wcl_horizontal_padding, 0.f});

						raid_instance_panel->add(krsi_bracket_bar_m, "krsi_top_bracket_bar_m");

						auto krsi_overall_bar_m = std::make_shared<tgui::ProgressBar>();
						krsi_overall_bar_m->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						krsi_overall_bar_m->setMinimum(0);
						krsi_overall_bar_m->setMaximum(100);
						krsi_overall_bar_m->setSize(style_theme::wcl_progress_bar_width, 14);
						krsi_overall_bar_m->setText("loading information..");
						krsi_overall_bar_m->setPosition(krsi_overall_bar_hc->getPosition() + sf::Vector2f{krsi_overall_bar_hc->getSize().x + style_theme::wcl_horizontal_padding, 0.f});

						raid_instance_panel->add(krsi_overall_bar_m, "krsi_top_overall_bar_m");

						// HC Median Bars

						auto krsi_median_bracket_bar_hc = std::make_shared<tgui::ProgressBar>();
						krsi_median_bracket_bar_hc->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						krsi_median_bracket_bar_hc->setMinimum(0);
						krsi_median_bracket_bar_hc->setMaximum(100);
						krsi_median_bracket_bar_hc->setSize(style_theme::wcl_progress_bar_width, 14);
						krsi_median_bracket_bar_hc->setText("tbi [median bracket]");
						krsi_median_bracket_bar_hc->setPosition(krsi_overall_bar_hc->getPosition() + sf::Vector2f{0.f, krsi_overall_bar_hc->getSize().y + raid_instance_panel_inside_padding * 2});

						raid_instance_panel->add(krsi_median_bracket_bar_hc, "krsi_median_bracket_bar_hc");

						auto krsi_median_overall_bar_hc = std::make_shared<tgui::ProgressBar>();
						krsi_median_overall_bar_hc->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						krsi_median_overall_bar_hc->setMinimum(0);
						krsi_median_overall_bar_hc->setMaximum(100);
						krsi_median_overall_bar_hc->setSize(style_theme::wcl_progress_bar_width, 14);
						krsi_median_overall_bar_hc->setText("tbi [overall bracket]");
						krsi_median_overall_bar_hc->setPosition(krsi_median_bracket_bar_hc->getPosition() + sf::Vector2f{0.f, krsi_median_bracket_bar_hc->getSize().y + raid_instance_panel_inside_padding * 2});

						raid_instance_panel->add(krsi_median_overall_bar_hc, "krsi_median_overall_bar_hc");

						// M Median Bars

						auto krsi_median_bracket_bar_m = std::make_shared<tgui::ProgressBar>();
						krsi_median_bracket_bar_m->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						krsi_median_bracket_bar_m->setMinimum(0);
						krsi_median_bracket_bar_m->setMaximum(100);
						krsi_median_bracket_bar_m->setSize(style_theme::wcl_progress_bar_width, 14);
						krsi_median_bracket_bar_m->setText("tbi [median bracket]");
						krsi_median_bracket_bar_m->setPosition(krsi_median_bracket_bar_hc->getPosition() + sf::Vector2f{krsi_median_bracket_bar_hc->getSize().x + style_theme::wcl_horizontal_padding, 0.f});

						raid_instance_panel->add(krsi_median_bracket_bar_m, "krsi_median_bracket_bar_m");

						auto krsi_median_overall_bar_m = std::make_shared<tgui::ProgressBar>();
						krsi_median_overall_bar_m->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
						krsi_median_overall_bar_m->setMinimum(0);
						krsi_median_overall_bar_m->setMaximum(100);
						krsi_median_overall_bar_m->setSize(style_theme::wcl_progress_bar_width, 14);
						krsi_median_overall_bar_m->setText("tbi [overall bracket]");
						krsi_median_overall_bar_m->setPosition(krsi_median_overall_bar_hc->getPosition() + sf::Vector2f{krsi_median_overall_bar_hc->getSize().x + style_theme::wcl_horizontal_padding, 0.f});

						raid_instance_panel->add(krsi_median_overall_bar_m, "krsi_median_overall_bar_m");

						anchor_position = krsi_median_overall_bar_hc->getPosition() + sf::Vector2f{0.f, krsi_median_overall_bar_hc->getSize().y + raid_instance_panel_section_padding};
					}
				}

				++num_x;
			}

			get_gui().add(raid_main_layout, "raid_main_layout");
		}

		// Finish / wait for the warcraftlogs ranking job
		warcraftlogs_top_job.get();

		for (auto& raid : *local_raiddata->get_raids()) {
			const auto it = std::find(wcl_selected_raids.cbegin(), wcl_selected_raids.cend(), raid.m_wcl_zone_id);

			if (it == wcl_selected_raids.cend()) {
				continue;
			}

			raid.m_wcl_per_boss_top_rankings = std::move(warcraftlogs_raiddata_rankings_copy.get_raid(raid.m_raid_name)->m_wcl_per_boss_top_rankings);

			// Calculate total
			float accumulated_percentage = 0.f;
			unsigned int total_bosses = 0;
			for (const auto& boss_ranking : raid.m_wcl_per_boss_top_rankings) {
				if (!boss_ranking.has_avg_pct(metric::DPS, boss_difficulty::heroic, false)) {
					continue;
				}
				
				const auto boss_avg = boss_ranking.get_avg_pct(metric::DPS, boss_difficulty::heroic, false);

				//std::cout << "boss " << boss_ranking.m_boss_name.toAnsiString() << " avg: " << boss_avg << std::endl;

				accumulated_percentage += boss_avg;
				++total_bosses;
			}

			const float avg_percentage = accumulated_percentage / static_cast<float>(total_bosses);

			tgui::Panel* raid_instance_panel = dynamic_cast<tgui::Panel*>(get_gui().get(raid.m_raid_name + "_panel", true).get());
			tgui::ProgressBar* bar = dynamic_cast<tgui::ProgressBar*>(raid_instance_panel->get("dps_top_overall_bar_hc").get());
			bar->setValue(static_cast<unsigned int>(std::round(avg_percentage)));
			bar->setText("top overall - hc");

			//std::cout << "Total percentage for " << raid.m_raid_name.toAnsiString() << ": " << avg_percentage << std::endl;
		}

		// Finish / wait for the warcraftlogs parse job .. this may take time

		warcraftlogs_median_job.get();

		for (auto& raid : *local_raiddata->get_raids()) {
			raid.m_wcl_per_boss_median_rankings = std::move(warcraftlogs_raiddata_parse_copy.get_raid(raid.m_raid_name)->m_wcl_per_boss_median_rankings);
		}

		{
			auto lock = conc_lock<gui>(this);
		}

		m_cached_raiddata = std::move(local_raiddata);
	});
}

void main_gui::clear_raid_layout() {
	auto gui_lock = conc_lock<gui>(this);

	auto raid_main_layout = get_gui().get("raid_main_layout");

	if (raid_main_layout != nullptr) {
		get_gui().remove(raid_main_layout);
	}
}

void main_gui::build_wait_layout() {
	auto gui_lock = conc_lock<gui>(this);

	tgui::Panel::Ptr wait_layout = m_tgui_theme->load("WaitLayout");
	wait_layout->setPosition(style_theme::main_layout_padding, 120);
	wait_layout->setSize(get_central()->get_renderwindow()->getSize().x - style_theme::main_layout_padding * 2, get_gui().getWindow()->getSize().y - style_theme::main_layout_padding - wait_layout->getPosition().y);

	tgui::Label::Ptr wait_label = m_tgui_theme->load("WaitLabel");
	wait_label->setTextSize(24);
	wait_label->setText(">> waiting for input");
	wait_label->setPosition(sf::Vector2f{20.f, (wait_layout->getSize().y * 0.5f) - (wait_label->getSize().y * 0.5f)});

	wait_layout->add(wait_label, "wait_label");

	get_gui().add(wait_layout, "wait_layout");
}

void main_gui::remove_wait_layout() {
	auto gui_lock = conc_lock<gui>(this);

	auto wait_layout = get_gui().get("wait_layout");

	if (wait_layout != nullptr) {
		get_gui().remove(wait_layout);
	}
}

void main_gui::push_wait_message(sf::String message) {
	auto gui_lock = conc_lock<gui>(this);

	auto wait_label = get_gui().get("wait_label", true);

	dynamic_cast<tgui::Label*>(wait_label.get())->setText(std::move(message));
}

void main_gui::fill_armory_character_statistics_tooltip(tgui::Panel* tooltip, const wow_raid& raid, boss_difficulty bossdifficulty) {
	float widest_label = 0.f;
	unsigned int num_bosses = 0;
	for (const auto& statistic : raid.m_armory_boss_statistics) {
		const unsigned int numKills = [&statistic, &bossdifficulty] {
			switch (bossdifficulty) {
				case boss_difficulty::normal:
					return statistic.m_num_nhc_kills;
				case boss_difficulty::heroic:
					return statistic.m_num_hc_kills;
				case boss_difficulty::mythic:
					return statistic.m_num_m_kills;
				case boss_difficulty::lfr:
				default:
					throw ts_exception{"main_gui::fill_armory_character_statistics_tooltip(): illegal boss difficulty"};
					break;
			}
		}();

		auto wstr = std::wstringstream{};
		wstr << L"[" << numKills << L"x] " << statistic.m_boss_name.toWideString();
		const auto full_label_text = wstr.str();

		tgui::Label::Ptr label = [this, &statistic, &numKills] {
			if (numKills > 0) {
				return m_tgui_theme->load("ToolTip_Killed");
			}
			else {
				return m_tgui_theme->load("ToolTip_Open");
			}
		}();

		label->setTextSize(style_theme::tooltip_text_size);
		label->setText(full_label_text);
		label->setPosition(2, num_bosses * style_theme::tooltip_text_size + 2);

		const auto label_width = label->getSize().x;
		if (label_width > widest_label) {
			widest_label = label_width;
		}

		tooltip->add(label);

		++num_bosses;
	}

	tooltip->setSize(widest_label * 2.f, num_bosses * style_theme::tooltip_text_size + 5.f);
}

} // namespace ts