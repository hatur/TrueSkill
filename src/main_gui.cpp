#include "ts_central.h"
#include "main_gui.h"

#include <memory>

#include "conc_lock.h"
#include "raid_data.h"
#include "style_theme.h"

namespace ts {

main_gui::main_gui(ts_central* ts_central)
	: gui{ts_central} {

}

void main_gui::build() {
	auto raiddata = std::make_shared<raid_data>();

	if (!raiddata->is_initialized()) {
		std::cout << "main_gui::build(): raiddata not properly initialized!" << std::endl;
		return;
	}

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
		auto realmlist = get_central()->get_mashery_client()->retrieve_realm_list(region_combo->getSelectedItem());

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

		//std::lock_guard<std::recursive_mutex> lock{get_mutex()};
		//server_combo->removeAllItems();
		//server_combo->setMaximumItems(realmlist.size());
		//for (const auto& realm : realmlist) {
		//	server_combo->addItem(realm.m_name);
		//}

		//server_combo->setSelectedItemByIndex(0);
	});

	auto name_edit = std::make_shared<tgui::EditBox>();
	name_edit->setPosition(server_search_field->getPosition().x + server_search_field->getSize().x + 4, server_search_field->getPosition().y);
	name_edit->setSize(160, 18);

	search_panel->add(name_edit);

	tgui::Button::Ptr search_button = std::make_shared<tgui::Button>();
	search_button->setPosition(name_edit->getPosition().x + name_edit->getSize().x + 4, name_edit->getPosition().y);
	search_button->setSize(64, 18);
	search_button->setText("search");

	search_button->connect("Pressed", [this, region_combo, server_search_field, name_edit, raiddata] {
		clear_raid_layout();

		// Maybe add waiting layout here
		build_wait_layout();

		auto result = get_central()->get_threadpool()->job([this, region_combo, server_search_field, name_edit, raiddata] {
			try {
				push_wait_message(">> retrieving armory data..");
				get_central()->get_mashery_client()->retrieve_raid_data(region_combo->getSelectedItem(), server_search_field->getText(), name_edit->getText(), raiddata);
				push_wait_message(">> retrieving warcraftlogs information (simulation)..");
				std::this_thread::sleep_for(std::chrono::seconds(3));
			}
			catch (const ts_exception& ex) {
				std::cout << "Couldn't retrieve armory data, reason: " << ex.what() << std::endl;
				push_wait_message(">> couldn't retrieve armory data (armory down?) try again..");
				return;
			}

			remove_wait_layout();

			auto lock = conc_lock<gui>(this);			

			{
				auto raid_lock = conc_lock<raid_data>(raiddata.get());

				tgui::Panel::Ptr raid_main_layout = m_tgui_theme->load("RaidOuter");
				raid_main_layout->setPosition(style_theme::main_layout_padding, 112);
				raid_main_layout->setSize(get_central()->get_renderwindow()->getSize().x - style_theme::main_layout_padding * 2
					, get_gui().getWindow()->getSize().y - style_theme::main_layout_padding - raid_main_layout->getPosition().y);

				int num_x = 0;
				for (const auto& raid : *raiddata->get_raids()) {
					const float panel_width = 200;
					const float raid_instance_panel_inside_padding = 2;
					const float raid_instance_panel_section_padding = 6;

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

					tgui::Label::Ptr mythic_armory_progress_label = m_tgui_theme->load("RaidInfo");
					mythic_armory_progress_label->setText("Mythic Account progression:");
					mythic_armory_progress_label->setPosition(raid_icon->getPosition() + sf::Vector2f{0.f, raid_icon->getSize().y} +sf::Vector2f{0.f, raid_instance_panel_section_padding});
					mythic_armory_progress_label->setTextSize(10);

					raid_instance_panel->add(mythic_armory_progress_label);

					auto mythic_armory_progress_bar = std::make_shared<tgui::ProgressBar>();
					mythic_armory_progress_bar->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
					mythic_armory_progress_bar->setMinimum(0);
					mythic_armory_progress_bar->setMaximum(100);
					mythic_armory_progress_bar->setSize(100, 12);
					mythic_armory_progress_bar->setPosition(mythic_armory_progress_label->getPosition() + sf::Vector2f{0.f, static_cast<float>(mythic_armory_progress_label->getTextSize()) + 2.f} +sf::Vector2f{0.f, raid_instance_panel_inside_padding});
					
					const float mythic_ach_progression = static_cast<float>(raid.get_armory_num_killed_bosses_acc()) * 100.f / static_cast<float>(raid.get_total_bosses());

					mythic_armory_progress_bar->setValue(static_cast<unsigned int>(mythic_ach_progression));

					raid_instance_panel->add(mythic_armory_progress_bar, "armory_acc_progress_bar");

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

						label->setTextSize(10);
						label->setText(full_label_text);
						label->setPosition(2, num_ach * 12.f);

						const auto label_width = label->getSize().x;
						if (label_width > widest_label) {
							widest_label = label_width;
						}

						mythic_armory_progress_bar_tooltip->add(label);

						++num_ach;
					}

					mythic_armory_progress_bar_tooltip->setSize(widest_label * 2.f, num_ach * 12.f);

					auto mythic_armory_progress_bar_tooltip_wrapper = std::make_unique<tooltip_wrapper>(&get_gui(), mythic_armory_progress_bar, mythic_armory_progress_bar_tooltip);
					register_tooltip(std::move(mythic_armory_progress_bar_tooltip_wrapper));

					++num_x;
				}

				get_gui().add(raid_main_layout, "raid_main_layout");
			}
		});
	});

	search_panel->add(search_button);
}

void main_gui::push_realm_list(std::vector<realm>&& realmlist) {
	auto lock = conc_lock<gui>(this);
	m_cached_realmlist = std::move(realmlist);
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

	get_gui().remove(wait_layout);

}

void main_gui::push_wait_message(sf::String message) {
	auto gui_lock = conc_lock<gui>(this);

	auto wait_label = get_gui().get("wait_label", true);

	dynamic_cast<tgui::Label*>(wait_label.get())->setText(std::move(message));
}

} // namespace ts