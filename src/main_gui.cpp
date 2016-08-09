#include "ts_central.h"
#include "main_gui.h"

#include <memory>

#include "conc_lock.h"
#include "raid_data.h"

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

	const int panel_padding_outer = 8;

	auto lock = conc_lock<gui>(this);

	auto theme = std::make_shared<tgui::Theme>("data/theme/default.tg");

	//tgui::ToolTip::setTimeToDisplay(sf::seconds(2));
	//tgui::ToolTip::setDistanceToMouse({10.f, 10.f});

	tgui::Panel::Ptr search_panel = theme->load("SearchPanel");
	search_panel->setPosition(panel_padding_outer, panel_padding_outer);
	search_panel->setSize(sf::Vector2f{get_central()->get_renderwindow()->getSize()}.x - panel_padding_outer * 2, 120 - panel_padding_outer * 2);

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
		auto result = get_central()->get_threadpool()->job([this, region_combo, server_search_field, name_edit, raiddata] {
			try {
				get_central()->get_mashery_client()->retrieve_raid_data(region_combo->getSelectedItem(), server_search_field->getText(), name_edit->getText(), raiddata);
			}
			catch (const ts_exception& ex) {
				std::cout << "Couldn't retrieve armory data, reason: " << ex.what() << std::endl;
				return;
			}

			auto lock = conc_lock<gui>(this);

			{
				auto raid_lock = conc_lock<raid_data>(raiddata.get());

				auto* raid_main_panel = dynamic_cast<tgui::Panel*>(get_gui().get("raid_main_layout").get());
				auto widgetNames = raid_main_panel->getWidgetNames();
				for (const auto& name : widgetNames) {
					std::cout << "Widget with name " << name.toAnsiString() << std::endl;

					auto raid_name = name;

					raid_name.replace(sf::String{"_panel"}, sf::String{""});
					auto* raid = raiddata->get_raid(raid_name);

					auto* raid_instance_panel = dynamic_cast<tgui::Panel*>(raid_main_panel->get(name).get());
					auto* acc_progress_bar = dynamic_cast<tgui::ProgressBar*>(raid_instance_panel->get("armory_acc_progress_bar").get());

					const float mythic_ach_progression = static_cast<float>(raid->get_armory_num_killed_bosses_acc()) * 100.f / static_cast<float>(raid->get_total_bosses());
					
					std::cout << "mythic progress: " << mythic_ach_progression << " percent" << std::endl;

					acc_progress_bar->setValue(static_cast<unsigned int>(mythic_ach_progression));
				}
			}
		});
	});

	search_panel->add(search_button);

	auto tooltip = std::make_shared<tgui::Label>();
	tooltip->setText("LUL PLS");
	tooltip->setSize(54, 54);

	auto button = std::make_shared<tgui::Button>();
	button->setText(sf::String("Switch"));
	button->setPosition(0, get_central()->get_renderwindow()->getSize().y - button->getSize().y);
	button->setToolTip(tooltip);
	//button->askToolTip({500, 500});
	//button->connect("pressed", [this] { show(gui_type::test_screen); });

	get_gui().add(button);

	tgui::Panel::Ptr raid_main_layout = theme->load("RaidOuter");
	raid_main_layout->setPosition(panel_padding_outer, 120);
	raid_main_layout->setSize(get_central()->get_renderwindow()->getSize().x - panel_padding_outer * 2, 500);

	{
		// Here we build the raid info gui

		auto lock = conc_lock<raid_data>(raiddata.get());
		int num_x = 0;
		for (const auto& raid : *raiddata->get_raids()) {
			const float panel_width = 200;
			const float raid_instance_panel_inside_padding = 2;
			const float raid_instance_panel_section_padding = 6;

			tgui::Panel::Ptr raid_instance_panel = theme->load("RaidInner");
			raid_main_layout->add(raid_instance_panel, raid.m_raid_name + "_panel");
			raid_instance_panel->setPosition(4 + num_x * panel_width + num_x * 4, 4);
			raid_instance_panel->setSize(panel_width, raid_instance_panel->getParent()->getSize().y - 8);

			auto raid_icon = std::make_shared<tgui::Picture>();
			raid_icon->setTexture(raid.m_icon_path);
			raid_icon->setPosition(raid_instance_panel_inside_padding, raid_instance_panel_inside_padding);
			raid_icon->setSize(48, 48);
			raid_icon->setSmooth(true);

			raid_instance_panel->add(raid_icon);

			tgui::Label::Ptr raid_name = theme->load("RaidHeading");
			raid_name->setText(raid.m_raid_name);
			raid_name->setPosition(raid_icon->getPosition().x + raid_icon->getSize().x + raid_instance_panel_inside_padding, 0);
			raid_name->setTextSize(16);

			raid_instance_panel->add(raid_name);

			tgui::Label::Ptr mythic_armory_progress_label = theme->load("RaidInfo");
			mythic_armory_progress_label->setText("Mythic Account progression:");
			mythic_armory_progress_label->setPosition(raid_icon->getPosition() + sf::Vector2f{0.f, raid_icon->getSize().y} + sf::Vector2f{0.f, raid_instance_panel_section_padding});
			mythic_armory_progress_label->setTextSize(10);

			raid_instance_panel->add(mythic_armory_progress_label);

			auto mythic_armory_progress_tooltip = std::make_shared<tgui::Picture>("data/img/highmaul.png");

			auto mythic_armory_progress_bar = std::make_shared<tgui::ProgressBar>();
			mythic_armory_progress_bar->setFillDirection(tgui::ProgressBar::FillDirection::LeftToRight);
			mythic_armory_progress_bar->setMinimum(0);
			mythic_armory_progress_bar->setMaximum(100);
			mythic_armory_progress_bar->setSize(100, 12);
			mythic_armory_progress_bar->setPosition(mythic_armory_progress_label->getPosition() + sf::Vector2f{0.f, static_cast<float>(mythic_armory_progress_label->getTextSize()) + 2.f} + sf::Vector2f{0.f, raid_instance_panel_inside_padding});
			mythic_armory_progress_bar->setValue(0);
			mythic_armory_progress_bar->setToolTip(mythic_armory_progress_tooltip);

			raid_instance_panel->add(mythic_armory_progress_bar, "armory_acc_progress_bar");

			++num_x;
		}
	}

	get_gui().add(raid_main_layout, "raid_main_layout");
}

void main_gui::push_realm_list(std::vector<realm>&& realmlist) {
	auto lock = conc_lock<gui>(this);
	m_cached_realmlist = std::move(realmlist);
}

} // namespace ts