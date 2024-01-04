#define _WINSOCKAPI_  
#include <stdafx.h>
#include <core/injector/event_handler.hpp>

#include <d3dx9tex.h>
#include <d3dx9.h>

#include <window/core/window.hpp>
#include <window/core/dx9_image.hpp>
#include <window/win_handler.hpp>
#include <window/interface/globals.h>

#include <utils/http/http_client.hpp>
#include <utils/thread/thread_handler.hpp>

#include <window/api/installer.hpp>
#include <window/core/colors.hpp>

#include <nlohmann/json.hpp>
#include <filesystem>
#include <set>

#include <core/steam/cef_manager.hpp>

struct render
{
private:

	nlohmann::basic_json<> m_itemSelectedSource;

	const void listButton(const char* name, int index) {
		static ImU32 col = ImGui::GetColorU32(ImGuiCol_CheckMark);

		if (ui::current_tab_page == index) {
			if (ImGui::Button(name)) ui::current_tab_page = index;
			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			if (ImGui::IsItemClicked(1)) ImGui::OpenPopup(std::format("selectable_{}", index).c_str());
		}
		else {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
			if (ImGui::Button(name)) ui::current_tab_page = index;
			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			if (ImGui::IsItemClicked(1)) ImGui::OpenPopup(std::format("selectable_{}", index).c_str());
			ImGui::PopStyleColor(1);
		}
	};
public:

	struct updateItem
	{
		std::string status;
		int id = -1;
	};

	nlohmann::json m_editObj;
	bool m_editMenuOpen = false;

	struct ColorInfo {
		std::string name;
		ImVec4 color;
		ImVec4 original;
		std::string comment;
	};
	std::vector<ColorInfo> colorList;

	void openPopupMenu(nlohmann::basic_json<>& skin)
	{
		colorList.clear();
		//const auto skin = config.getThemeData(false);

		//try to parse colors from the skin object
		if (skin.contains("GlobalsColors") && skin.is_object())
		{
			for (const auto& color : skin["GlobalsColors"])
			{
				if (!color.contains("ColorName") || !color.contains("HexColorCode") || !color.contains("OriginalColorCode") || !color.contains("Description"))
				{
					console.err("Couldn't collect global color. 'ColorName' or 'HexColorCode' or 'OriginalColorCode' or 'Description' doesn't exist");
					continue;
				}

				colorList.push_back({
					color["ColorName"],
					colors::HexToImVec4(color["HexColorCode"]),
					colors::HexToImVec4(color["OriginalColorCode"]),
					color["Description"]
				});
			}
		}
		else {
			console.log("Theme doesn't have GlobalColors");
		}

		m_editObj = skin;
		m_editMenuOpen = true;
	}

	void deleteListing(nlohmann::basic_json<>& skin) {
		int result = MessageBoxA(GetForegroundWindow(), std::format("Are you sure you want to delete {}?\nThis cannot be undone.", skin["native-name"].get<std::string>()).c_str(), "Confirmation", MB_YESNO | MB_ICONINFORMATION);
		if (result == IDYES)
		{
			std::string disk_path = std::format("{}/{}", config.getSkinDir(), skin["native-name"].get<std::string>());
			if (std::filesystem::exists(disk_path)) {

				try {
					std::filesystem::remove_all(std::filesystem::path(disk_path));
				}
				catch (const std::exception& ex) {
					MsgBox(std::format("Couldn't remove the selected skin.\nError:{}", ex.what()).c_str(), "Non-fatal Error", MB_ICONERROR);
				}
			}
			m_Client.parseSkinData(false);
		}
	}

	void createLibraryListing(nlohmann::basic_json<> skin, int index, bool deselect = false)
	{
		const std::string m_skinName = skin.value("native-name", std::string());

		static bool push_popped = false;
		static int hovering;

		bool popped = false;
		//remove window padding to allow the title bar child to hug the inner sides
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		//get heigth of description text so we can base the height of the modal relative to that
		float desc_height = ImGui::CalcTextSize(skin["description"].get<std::string>().c_str(), 0, false, ImGui::GetContentRegionAvail().x - 50).y;

		if (hovering == index) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
			push_popped = true;
		}
		else ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

		static bool btn1hover = false;
		static bool btn2hover = false;
		static bool btn3hover = false;

		if (m_Client.m_currentSkin == m_skinName)
			ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_CheckMark));
		else
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.25f, 0.25f, 0.0f));

		bool requiresUpdate = skin.value("update_required", false);

		ImGui::BeginChild(std::format("card_child_container_{}", index).c_str(), ImVec2(rx, 35/*85 + desc_height*/), true, ImGuiWindowFlags_ChildWindow);
		{
			//reset the window padding rules
			ImGui::PopStyleVar();
			popped = true;
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6);
			ImGui::PopStyleColor();

			//display the title bar of the current skin 
			ImGui::BeginChild(std::format("skin_header{}", index).c_str(), ImVec2(rx, 35), true, ImGuiWindowFlags_NoScrollbar);
			{
				ImGui::Image((void*)Window::iconsObj().skin_icon, ImVec2(ry - 1, ry - 1));
				ImGui::SameLine();
				ui::shift::x(-4);
				ImGui::Text(skin.value("name", "null").c_str());
				ImGui::SameLine();
				ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() - 5, ImGui::GetCursorPosY() + 1));
				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), std::format("{} by {}", skin.value("version", "1.0.0"), skin.value("author", "unknown")).c_str());
				ImGui::SameLine();
				ui::shift::right(35);
				ImGui::SameLine();
				ui::shift::y(1);
				ImGui::PopFont();

				if (requiresUpdate)
				{
					if (hovering == index)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_CheckMark));

						ui::shift::y(-3);
						ui::shift::right(159);

						ImGui::ImageButton(Window::iconsObj().editIcon, ImVec2(16, 16));
						if (ImGui::IsItemHovered()) {
							btn1hover = true;
						}
						else {
							btn1hover = false;
						}
						if (ImGui::IsItemClicked()) {
							this->openPopupMenu(skin);
						}

						ImGui::SameLine(0);
						ui::shift::x(-4);
						ImGui::PopStyleColor(2);

						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
						ImGui::ImageButton(Window::iconsObj().deleteIcon, ImVec2(16, 16));
						if (ImGui::IsItemHovered()) {
							btn2hover = true;
						}
						else {
							btn2hover = false;
						}
						if (ImGui::IsItemClicked()) {
							deleteListing(skin);
						}

						ImGui::PopStyleColor(2);
						ImGui::SameLine();
						ui::shift::x(-4);
					}
					else {
						ui::shift::y(-3);
						ui::shift::right(95);
					}

					ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_CheckMark));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_CheckMark));

					if (ImGui::Button("UPDATE", ImVec2(rx, 0))) {
						community::_installer->downloadTheme(skin);
						m_Client.parseSkinData(false, true, skin["native-name"]);
					}
					if (ImGui::IsItemHovered()) {
						btn3hover = true;

						std::string message = skin.contains("git") 
							&& skin["git"].contains("message") 
							&& !skin["git"]["message"].is_null() ? 
								skin["git"].value("message", "null") : "null";

						std::string date = skin.contains("git") 
							&& skin["git"].contains("date") 
							&& !skin["git"]["date"].is_null() ? 
								skin["git"].value("date", "null") : "null";

						std::string text = std::format("{} was updated on {}.\n\nReason:\n{}\n\nMiddle click to see more details...", skin.value("native-name", "null"), date, message);

						ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.15f, .15f, .15f, 1.f));
						ImGui::SetTooltip(text.c_str());
						ImGui::PopStyleColor();
					}
					else {
						btn3hover = false;
					}

					if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) {
						OpenURL((skin.contains("git") && skin["git"].contains("url") && !skin["git"]["url"].is_null() ? skin["git"]["url"].get<std::string>() : "null").c_str());
					}

					ImGui::PopStyleColor(2);
				}
				else if (hovering == index)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_CheckMark));

					ui::shift::y(-3);
					ui::shift::right(55);

					ImGui::ImageButton(Window::iconsObj().editIcon, ImVec2(16, 16));
					if (ImGui::IsItemHovered()) btn1hover = true;
					else                        btn1hover = false;

					if (ImGui::IsItemClicked()) 
						this->openPopupMenu(skin);

					ImGui::SameLine(0);
					ui::shift::x(-4);
					ImGui::PopStyleColor(2);

					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
					ImGui::ImageButton(Window::iconsObj().deleteIcon, ImVec2(16, 16));

					if (ImGui::IsItemHovered()) btn2hover = true;
					else                        btn2hover = false;
					
					if (ImGui::IsItemClicked()) this->deleteListing(skin);
					
					ImGui::PopStyleColor(2);
				}
			}
			ImGui::EndChild();
			ImGui::PopStyleVar();		
		}


		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
			std::cout << "calling to change skin..." << std::endl;
			m_Client.changeSkin((nlohmann::basic_json<>&)skin);
		}

		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			if (true/*!btn1hover || !btn2hover || !btn3hover*/) {
				hovering = index;
			} else {
				hovering = -1;
			}

			push_popped = false;
		}
		else if (push_popped) hovering = -1;

		ImGui::PopStyleColor();
		ImGui::EndChild();
	}
	void library_panel()
	{
		//if (ImGui::Button("Testing Button"))
		//{
		//	clientMessagePopup("Hello", "Hi!");
		//}

		if (m_Client.m_missingSkins > 0)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, .5f), std::format("{} missing remote skin(s). couldn't connect to endpoint", m_Client.m_missingSkins).c_str());
			ImGui::SameLine();
		}
		if (m_Client.m_unallowedSkins > 0)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, .5f), std::format("{} missing remote skin(s). networking not allowed", m_Client.m_unallowedSkins).c_str());
			ImGui::SameLine();
		}

		static char text_buffer[150];

		//ImGui::BeginChild("HeaderDrag", ImVec2(rx, 35), false);
		//{
		//	if (ImGui::IsWindowHovered())
		//	{
		//		g_headerHovered = true;
		//	}
		//	else
		//	{
		//		g_headerHovered = false;
		//	}
		//}
		//ImGui::EndChild();

		auto worksize = ImGui::GetMainViewport()->WorkSize;

		float child_width = (float)(worksize.x < 800 ? rx : worksize.x < 1400 ? rx / 1.2 : worksize.x < 1800 ? rx / 1.4 : rx / 1.5);

		//ImGui::Spacing();
		//ImGui::Spacing();
		//ImGui::Spacing();

		ui::center(rx, child_width, 8);

		static float contentHeight = 100.0f; // Initialize with a minimum height

		if (g_fileDropQueried) {
			ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_CheckMark));
		}

		ImGui::BeginChild("###library_container", ImVec2(child_width, contentHeight), true, ImGuiWindowFlags_AlwaysAutoResize);
		{
			//try {

			//}
			//catch (nlohmann::detail::exception& except) {
			//	console.err(except.what())
			//}

			if (g_fileDropQueried) {
				ImGui::PopStyleColor();
			}

			static const auto to_lwr = [](std::string str) {
				std::string result;
				for (char c : str) {
					result += std::tolower(c);
				}
				return result;
			};

			bool skinSelected = false;

			for (size_t i = 0; i < m_Client.skinData.size(); ++i)
			{
				nlohmann::basic_json<>& skin = m_Client.skinData[i];

				if (skin.value("native-name", std::string()) == m_Client.m_currentSkin)
				{
					skinSelected = true;

					//ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
					//{
					//	ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "SELECTED:");
					//}
					//ImGui::PopFont();

					createLibraryListing(skin, i, true);

					ImGui::Spacing();
					ImGui::Spacing();
				}
			}

			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
			{
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "LIBRARY:");
			}
			ImGui::PopFont();

			ImGui::SameLine();

			ui::shift::right(216);

			if (ImGui::ImageButton(Window::iconsObj().reload_icon, ImVec2(17, 17))) {
				m_Client.parseSkinData(false);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
				ImGui::SetTooltip("Reload Library...");
				ImGui::PopStyleColor();
			}

			ImGui::SameLine(); 
			ui::shift::x(-4);

			if (ImGui::ImageButton(Window::iconsObj().foldericon, ImVec2(17, 17))) {
				ShellExecuteA(NULL, "open", config.getSkinDir().c_str(), NULL, NULL, SW_SHOWNORMAL);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
				ImGui::SetTooltip("Open the skins folder...");
				ImGui::PopStyleColor();
			}

			ImGui::SameLine();
			ui::shift::x(-4);

			static int p_sortMethod = 0;

			//static const char* items[] = { "All Items", "Installed", "Cloud" };

			//ImGui::PushItemWidth(115);
			//if (ImGui::Combo(std::format("Sort").c_str(), &p_sortMethod, items, IM_ARRAYSIZE(items)));
			//ImGui::PopItemWidth();

			ImGui::PushItemWidth(150);

			auto position = ImGui::GetCursorPos();
			ImGui::InputText("##myInput", text_buffer, sizeof(text_buffer));
			static const auto after = ImGui::GetCursorPos();

			static bool is_focused = ImGui::IsItemActive() || ImGui::IsItemFocused();

			if (!is_focused && text_buffer[0] == '\0')
			{
				ImGui::SetCursorPos(ImVec2(position.x + 5, position.y + 2));

				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
				ImGui::TextUnformatted("Search Library...");
				ImGui::PopStyleColor();
			}

			//ImGui::SetCursorPos(after);
			ImGui::PopItemWidth();
			ui::shift::y(5);

			if (!skinSelected ? m_Client.skinData.empty() : m_Client.skinData.size() - 1 <= 0) {
				ui::shift::y((int)(ry / 2 - 25));

				ui::center(0, 240, 0);
				ImGui::BeginChild("noResultsContainer", ImVec2(240, 135), false);
				{
					ImGui::Image(Window::iconsObj().icon_no_results, ImVec2(240, 110));

					const char* text = "You don't have any themes!";

					ui::center(0, ImGui::CalcTextSize(text).x, 0);
					ImGui::Text(text);
				}
				ImGui::EndChild();
			}
			else {

				try {
					std::sort(m_Client.skinData.begin(), m_Client.skinData.end(), ([&](const nlohmann::json& a, const nlohmann::json& b) {
						bool downloadA = a.value("update_required", false);
						bool downloadB = b.value("update_required", false);

						if (downloadA && !downloadB) {
							return true;
						}
						else if (!downloadA && downloadB) {
							return false;
						}
						else {
							return a < b;
						}
						}));
				}
				catch (std::exception&) {

				}

				for (size_t i = 0; i < m_Client.skinData.size(); ++i)
				{
					nlohmann::basic_json<>& skin = m_Client.skinData[i];

					if (p_sortMethod == 1 && skin["remote"])
						continue;

					if (p_sortMethod == 2 && !skin["remote"])
						continue;

					if (skin.value("native-name", std::string()) != m_Client.m_currentSkin &&
						to_lwr(skin["name"].get<std::string>()).find(to_lwr(text_buffer)) != std::string::npos)
					{
						createLibraryListing(skin, i + (m_Client.skinData.size() + 1));
					}
				}
			}

			contentHeight = ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y;
		}
		ImGui::EndChild();
	}
	void settings_panel()
	{
		//ImGui::BeginChild("HeaderDrag", ImVec2(rx, 35), false);
		//{
		//	if (ImGui::IsWindowHovered())
		//	{
		//		g_headerHovered = true;
		//	}
		//	else
		//	{
		//		g_headerHovered = false;
		//	}
		//}
		//ImGui::EndChild();

		static steam_js_context SteamJSContext;

		auto worksize = ImGui::GetMainViewport()->WorkSize;
		float child_width = (float)(worksize.x < 800 ? rx - 20 : worksize.x < 1400 ? rx / 1.2 : worksize.x < 1800 ? rx / 1.4 : rx / 1.5);

		static float windowHeight = 0;

		ui::center(rx, child_width, -1);

		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 15);

		ImGui::BeginChild("settings_panel", ImVec2(child_width, windowHeight), false);
		{
			//ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

			static bool enable_css = Settings::Get<bool>("allow-stylesheet");

			ui::render_setting(
				"StyleSheet Insertion", "Allow CSS (StyleSheet) insertions in the skins you use. In case you only want the JavaScript features of the theme and don't want the CSS customizations",
				enable_css, false,
				[=]() {
					Settings::Set("allow-stylesheet", enable_css);
					SteamJSContext.reload();
				}
			);

			ImGui::Spacing(); ImGui::Spacing();

			static bool enable_js = Settings::Get<bool>("allow-javascript");

			ui::render_setting(
				"Javascript Execution", "Allow Javascript executions in the skins you use.\nJS can help with plethora of things inside Steam especially when customizing Steams look.\n\nJavascript can be malicious in many ways so\nonly allow JS execution on skins of authors you trust, or have manually reviewed the code",
				enable_js, false,
				[=]() {
					Settings::Set("allow-javascript", enable_js);
					SteamJSContext.reload();
				}
			);

			ImGui::Spacing(); ImGui::Spacing();

			static bool enable_store = Settings::Get<bool>("allow-store-load");

			ui::render_setting(
				"Enable Networking", "Controls whether HTTP requests are allowed which affects the Millennium Store and cloud hosted themes in case you want to opt out.\nThe store/millennium collects no identifiable data on the user, in fact, it ONLY collects download count.\n\nMillennium stays completely offline with this disabled.",
				enable_store, false,
				[=]() { Settings::Set("allow-store-load", enable_store); }
			);


			ImGui::Spacing(); ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing(); ImGui::Spacing();

			static const char* items[] = { "Top Left"/*0*/, "Top Right"/*1*/, "Bottom Left"/*2*/, "Bottom Right"/*3*/ };

			static int notificationPos = nlohmann::json::parse(SteamJSContext.exec_command("SteamUIStore.WindowStore.SteamUIWindows[0].m_notificationPosition.position"))["result"]["value"];

			ui::render_setting_list(
				"Client Notifications Position", "Adjusts the position of the client location instead of using its native coordinates. Displaying in the top corners is slightly broken",
				notificationPos, items, IM_ARRAYSIZE(items), false,
				[=]() {
					Settings::Set("NotificationsPos", nlohmann::json::parse(SteamJSContext.exec_command(std::format("SteamUIStore.WindowStore.SteamUIWindows[0].m_notificationPosition.position = {}", notificationPos)))["result"]["value"].get<int>());
				}
			);

			ImGui::Spacing();
			ImGui::Spacing();

			static bool enableUrlBar = Settings::Get<bool>("enableUrlBar");

			ui::render_setting(
				"Store URL Bar", "Force hide the store url bar that displays current location",
				enableUrlBar, true,
				[=]() {
					Settings::Set("enableUrlBar", enableUrlBar);
					SteamJSContext.reload();
				}
			);

			//ImGui::Spacing(); ImGui::Spacing();
			//ImGui::Separator();
			//ImGui::Spacing(); ImGui::Spacing();

			//static bool checkForUpdates = Settings::Get<bool>("allow-auto-updates");

			//ui::render_setting(
			//	"Auto check for Updates", "Millennium will periodically check for updates in the background and prompt to update if one is found.\nThis doesn't effect Steams performance.",
			//	checkForUpdates, true,
			//	[=]() {
			//		Settings::Set("allow-auto-updates", checkForUpdates);
			//		//SteamJSContext.reload();
			//	}
			//);

			//ImGui::Spacing(); ImGui::Spacing();

			//static bool updateSound = Settings::Get<bool>("allow-auto-updates-sound");

			//ui::render_setting(
			//	"Enable Notification Sounds", "Millennium will play a notification sound to help direct the attention to the notification displayed. The volume of the sound is the same as normal Steam notifications.\nThis does NOT effect actual Steam notification sounds.",
			//	updateSound, true,
			//	[=]() {
			//		Settings::Set("allow-auto-updates-sound", updateSound);
			//		//SteamJSContext.reload();
			//	}
			//);
			windowHeight = ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y;
		}
		ImGui::EndChild();

		ImGui::PopStyleVar();
	}
	void loadSelection(MillenniumAPI::resultsSchema& item)
	{
		//m_Client.releaseImages();

		m_Client.m_resultsSchema = item;
		m_Client.getRawImages(item.v_images);
		m_Client.b_showingDetails = true;
		m_Client.m_imageIndex = 0;

		console.log(item.skin_json);

		/*std::thread([=]() {*/

			try
			{
				std::string skinJsonResponse = http::get(m_Client.m_resultsSchema.skin_json);

				if (!nlohmann::json::accept(skinJsonResponse))
				{
					MsgBox(std::format("Json couldn't be parsed that was received from the remote server\n{}", 
						m_Client.m_resultsSchema.skin_json).c_str(), "Millennium", MB_ICONERROR);
					return;
				}

				this->m_itemSelectedSource = nlohmann::json::parse(skinJsonResponse);
			}
			catch (const http_error&) {
				MsgBox("Couldn't GET the skin data from the remote server", "Millennium", MB_ICONERROR);
				return;
			}
		/*}).detach();*/
	}
	void create_card(MillenniumAPI::resultsSchema& item, int index)
	{
		static bool push_popped = false;
		static int hovering;

		try
		{
			ImVec2 availableSpace = ImGui::GetContentRegionAvail();

			float desiredItemSize = (rx / 3) > 400 ? 400 : rx / 3;
			int columns = static_cast<int>(availableSpace.x / desiredItemSize);
			if (columns < 1) {
				columns = 1;
			}

			float actualItemSize = (availableSpace.x - (columns - 1) * ImGui::GetStyle().ItemSpacing.x) / columns;

			if (index > 0 && index % columns != 0) ImGui::SameLine();

			if (hovering == index)
			{
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
				push_popped = true;
			}
			else
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

			int height = item.image_list.height;
			int width = item.image_list.width;

			image::size result = image::maintain_aspect_ratio(width, height, (int)actualItemSize, (int)desiredItemSize);

			ImGui::BeginChild(("Child " + std::to_string(index)).c_str(), ImVec2(actualItemSize, result.height + 80.0f), true, ImGuiWindowFlags_None);
			{
				ImGui::PopStyleVar();

				if (item.image_list.texture != nullptr && item.image_list.texture->AddRef())
				{
					ImGui::Image(item.image_list.texture, ImVec2((float)result.width, (float)result.height));
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					hovering = index;
					push_popped = false;
				}
				else if (push_popped) hovering = -1;

				//if (ImGui::IsItemClicked())
				//{
				//	loadSelection(item);
				//}

				ui::shift::x(10);

				ImGui::BeginChild("###listing_description", ImVec2(rx, ry), false);
				{
					ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
					ImGui::Text(item.name.c_str());
					ImGui::PopFont();
					ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

					ImGui::SameLine();

					float width = ImGui::CalcTextSize(std::to_string(item.download_count).c_str()).x;

					ImGui::SameLine();
					ui::shift::right(30 + (int)width);
					//ui::shift::y(4);

					ImGui::Image(Window::iconsObj().download, ImVec2(15, 15));

					ImGui::SameLine();
					ui::shift::x(-6);
					//ui::shift::y(-4);
					ImGui::Text(std::to_string(item.download_count).c_str());

					ImGui::Text("by %s", item.gh_username.c_str());

					ImVec2 textSize = ImGui::CalcTextSize(item.description.c_str());
					ImVec2 availableSpace = ImGui::GetContentRegionAvail();

					if (textSize.x > availableSpace.x) {
						float ellipsisWidth = ImGui::CalcTextSize("...").x;
						float availableWidth = availableSpace.x - ellipsisWidth - 20;

						size_t numChars = 0;
						while (numChars < strlen(item.description.c_str()) && ImGui::CalcTextSize(item.description.c_str(), item.description.c_str() + numChars).x <= availableWidth) {
							numChars++;
						}

						ImGui::TextUnformatted(item.description.c_str(), item.description.c_str() + numChars);
						ImGui::SameLine(0);
						ui::shift::x(-12);
						ImGui::Text("...");
					}
					else {
						ImGui::TextUnformatted(item.description.c_str());
					}

					ImGui::PopStyleColor();
					ImGui::PopFont();
				}
				ImGui::EndChild();

				//if (ImGui::IsItemClicked())
				//{
				//	loadSelection(item);
				//}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					hovering = index;
					push_popped = false;
				}
				else if (push_popped) hovering = -1;
			}
			ImGui::EndChild();

			if (ImGui::IsItemClicked())
			{
				loadSelection(item);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				hovering = index;
				push_popped = false;
			}
			else if (push_popped) hovering = -1;


			ImGui::PopStyleColor();
		}
		catch (std::exception& ex)
		{
			console.err(ex.what());
		}
	}
	void store_panel()
	{
		//ImGui::BeginChild("HeaderDrag", ImVec2(rx, 35), false);
		//{
		//	if (ImGui::IsWindowHovered())
		//	{
		//		g_headerHovered = true;
		//	}
		//	else
		//	{
		//		g_headerHovered = false;
		//	}
		//}
		//ImGui::EndChild();

		auto worksize = ImGui::GetMainViewport()->WorkSize;
		static float windowHeight = 0;

		float child_width = (float)(worksize.x < 800 ? rx : worksize.x < 1400 ? rx / 1.2 : worksize.x < 1800 ? rx / 1.4 : rx / 1.5);

		if (api->isDown) {
			ImGui::Text("Oops!\nCommunity tab is currently down for maintenance. Check back later.");
		}
		else {
			if (api->get_query().size() == 0)
			{
				static const int width = 240;
				static const int height = 80;

				ImGui::SetCursorPos(ImVec2((rx / 2) - (width / 2), (ry / 2) - (height / 2)));

				if (ImGui::BeginChild("###store_page", ImVec2(width, height), false))
				{
					static int r = ImClamp(35, 0, 255);
					static int g = ImClamp(35, 0, 255);
					static int b = ImClamp(35, 0, 255);
					static int a = ImClamp(255, 0, 255);

					static ImU32 packedColor = static_cast<ImU32>((a << 24) | (b << 16) | (g << 8) | r);

					ImGui::Spacing();
					ui::center(rx, 30.0f, -9);
					ImGui::Spinner("###loading_spinner", 25.0f, 7, packedColor);

				}
				ImGui::EndChild();
			}
			else
			{
				if (!m_Client.b_showingDetails)
				{
					//ImGui::Spacing();
					//ImGui::Spacing();
					//ImGui::Spacing();

					ui::center(rx, child_width, 8);

					ImGui::BeginChild("##store_page_child", ImVec2(child_width, windowHeight), false);
					{
						//ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 150);
						ImGui::PushItemWidth(150);

						if (ImGui::ImageButton(Window::iconsObj().reload_icon, ImVec2(16, 16)))
						{
							api->retrieve_featured();
						}
						ImGui::SameLine();
						ui::shift::x(-4);

						static char text_buffer[150];
						auto position = ImGui::GetCursorPos();

						if (ImGui::InputText("##myInput", text_buffer, sizeof(text_buffer), ImGuiInputTextFlags_EnterReturnsTrue))
						{
							if (strlen(text_buffer) > 0) api->retrieve_search(text_buffer);
						}

						static const auto after = ImGui::GetCursorPos();

						if (!(ImGui::IsItemActive() || ImGui::IsItemFocused()) && text_buffer[0] == '\0')
						{
							ImGui::SetCursorPos(ImVec2(position.x + 5, position.y + 2));

							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
							ImGui::TextUnformatted("Search for themes...");
							ImGui::PopStyleColor();

							ImGui::SetCursorPos(after);
						}

						ImGui::PopItemWidth();

						ImGui::SameLine();

						ui::shift::right(250);

						auto query = api->get_query();

						static int sortByIndex = 4;
						static const char* sortByArray[] = { "Recently Added", "Alphabetical (A - Z)", "Alphabetical (Z - A)",  "Least Downloaded", "Most Downloaded" };

						ImGui::PushItemWidth(145);

						ImGui::Combo("###SortBy", &sortByIndex, sortByArray, IM_ARRAYSIZE(sortByArray));

						switch (sortByIndex)
						{
							using schema = MillenniumAPI::resultsSchema;

						case 0: { std::sort(query.begin(), query.end(), [&](const schema& a, const schema& b) { try { return std::stoi(a.date_added) > std::stoi(b.date_added); } catch (std::exception&) { return a.name < b.name; } }); break; }
						case 1: { std::sort(query.begin(), query.end(), [&](const schema& a, const schema& b) { return a.name < b.name; }); break; }
						case 2: { std::sort(query.begin(), query.end(), [&](const schema& a, const schema& b) { return a.name > b.name; }); break; }
						case 3: { std::sort(query.begin(), query.end(), [&](const schema& a, const schema& b) { return a.download_count < b.download_count; }); break; }
						case 4: { std::sort(query.begin(), query.end(), [&](const schema& a, const schema& b) { return a.download_count > b.download_count; }); break; }
						}

						ImGui::PopItemWidth();

						ImGui::SameLine();

						static std::set<std::string> categories = { "All" };
						std::vector<std::string> v_categories(categories.begin(), categories.end());

						ImGui::PushItemWidth(90);

						static int selectedItem = 0;

						if (ImGui::BeginCombo("###Category", selectedItem >= 0 ? v_categories[selectedItem].c_str() : nullptr)) {
							for (size_t i = 0; i < v_categories.size(); ++i) {
								bool isSelected = (i == selectedItem);
								if (ImGui::Selectable(v_categories[i].c_str(), isSelected)) {
									selectedItem = i;
								}
							}
							ImGui::EndCombo();
						}
						ImGui::PopItemWidth();

						int j = 0;
						for (auto& item : query)
						{
							for (const auto& tag : item.tags)
							{
								if (categories.find(tag) == categories.end()) {
									categories.insert(tag);
								}
							}

							if (v_categories[selectedItem] == "All") {
								create_card(item, j++);
							}
							else if (std::find(item.tags.begin(), item.tags.end(), v_categories[selectedItem]) != item.tags.end()) {
								create_card(item, j++);
							}
						}

						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
						ui::center_text(std::format("{} results found.", j).c_str());
						ImGui::PopStyleColor();

						windowHeight = ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y;
					}
					ImGui::EndChild();
				}
				else
				{
					//ImGui::Spacing();
					//ImGui::Spacing();
					//ImGui::Spacing();

					communityPaneDetails();
				}
			}
		}
	}
	void parse_text(const char* text)
	{
		std::istringstream stream(text);
		std::string line;

		const auto has_tag = ([=](std::string& str, const std::string& prefix) {
			bool has = str.substr(0, prefix.size()) == prefix;
			if (has) { str = str.substr(prefix.size()); }
			return has;
			});

		while (std::getline(stream, line, '\n')) {
			if (has_tag(line, "[Separator]")) { ImGui::Separator(); }
			else if (has_tag(line, "[Large]")) {
				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]); ImGui::TextWrapped(line.c_str()); ImGui::PopFont();
			}
			else {
				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); ImGui::TextWrapped(line.c_str()); ImGui::PopFont();
			}
		}
	}
	void communityPaneDetails()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 10);

		auto worksize = ImGui::GetMainViewport()->WorkSize;

		//set constraints on the children
		float child_width = (float)(worksize.x < 800 ? rx : worksize.x < 1400 ? rx / 1.2 : worksize.x < 1800 ? rx / 1.4 : rx / 1.5);

		static float windowHeight = 0;

		ui::center(rx, child_width, 8);
		ImGui::BeginChild("store_details", ImVec2(child_width, windowHeight), false);
		{
			ImGui::BeginChild("left_side", ImVec2(rx / 1.5f, ry), true);
			{
				if (ImGui::Button(" < Back "))
				{
					m_Client.b_showingDetails = false;

					//m_Client.releaseImages();
				}


				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

				ui::shift::y(10);

				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[4]);
				ui::center_text(m_Client.m_resultsSchema.name.c_str());
				ImGui::PopFont();

				ImGui::Spacing();

				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.5, .5, .5, 1));

				ui::center_text(std::format("By {} | Downloads {} | ID {}",
					m_Client.m_resultsSchema.gh_username,
					m_Client.m_resultsSchema.download_count,
					m_Client.m_resultsSchema.id
				).c_str());

				ImGui::PopStyleColor();
				ImGui::PopFont();

				ui::shift::y(10);

				ImGui::Separator();

				ImGui::Spacing();
				ImGui::Spacing();
				ImGui::Spacing();

				if (!m_Client.v_rawImageList.empty())
				{
					ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
					ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

					//get the height of the first element in the array and base the rest of the images height off the first one to prevent image jittering
					float ab_height = (float)image::maintain_aspect_ratio(
						m_Client.v_rawImageList[0].width,
						m_Client.v_rawImageList[0].height, (int)rx, (int)ry
					).height;

					//set max height barrier on the image node
					//ab_height = ab_height > (int)ry / 2 - 50 ? (int)ry / 2 - 50 : ab_height;

					//get width and height of the current image
					int height = m_Client.v_rawImageList[m_Client.m_imageIndex].height;
					int width = m_Client.v_rawImageList[m_Client.m_imageIndex].width;

					image::size result = image::maintain_aspect_ratio(width, height, (int)rx, (int)ab_height);

					float parent_width = rx - 20.0f;

					ui::center(rx, parent_width, 10);
					ImGui::BeginChild("image_parent_container", ImVec2(parent_width, (float)(ab_height + 40)), true);
					{
						ui::center(rx, (float)result.width, 1);
						ui::shift::y(20);

						ImGui::BeginChild("image_container", ImVec2((float)result.width, ab_height), true);
						{
							//check if the texture is useable
							if (m_Client.v_rawImageList[m_Client.m_imageIndex].texture != nullptr && m_Client.v_rawImageList[m_Client.m_imageIndex].texture->AddRef())
							{
								ImGui::Image(m_Client.v_rawImageList[m_Client.m_imageIndex].texture, ImVec2((float)result.width, ab_height));

								static bool imagePopupOpen = false;

								if (ImGui::IsItemClicked()) {
									imagePopupOpen = true;
								}

								ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.5f, 0.5f, 0.1f));

								if (ImGui::IsItemHovered())
								{
									ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(7, 7));
									ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
									ImGui::SetTooltip("Click to enlarge image.");
									ImGui::PopStyleVar();
								}

								if (imagePopupOpen && ImGui::Begin("Image Viewer", &imagePopupOpen, ImGuiWindowFlags_NoCollapse))
								{
									ImVec2 size = ImGui::GetWindowSize();

									image::size result = image::maintain_aspect_ratio(width, height, (int)size.x, (int)size.y);

									ImGui::SetCursorPosX((rx - result.width) / 2);
									ImGui::SetCursorPosY((ry - result.height) / 2);

									ImGui::Image(m_Client.v_rawImageList[m_Client.m_imageIndex].texture, ImVec2((float)result.width, (float)result.height));

									ui::center(rx, 90.0f, -1);

									ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10);
									ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);
									ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
									ImGui::BeginChild("carousel1", ImVec2(90, 20), true);
									{
										if (ImGui::Button(" < ", ImVec2(0, 17))) {
											if (m_Client.m_imageIndex - 1 >= 0) m_Client.m_imageIndex--;
										}
										ImGui::SameLine();
										ImGui::Text(std::format("{} of {}", m_Client.m_imageIndex + 1, m_Client.v_rawImageList.size()).c_str());
										ImGui::SameLine();
										ui::shift::right(20);
										if (ImGui::Button(" > ", ImVec2(0, 17)))
										{
											if (m_Client.m_imageIndex < ((int)m_Client.v_rawImageList.size() - 1))
												m_Client.m_imageIndex++;
										}
									}
									ImGui::EndChild();
									ImGui::PopStyleColor();

									ImGui::PopStyleVar(2);
									ImGui::End();
								}
								ImGui::PopStyleColor();
							}
						}
						ImGui::EndChild();
					}
					ImGui::EndChild();

					ImGui::Spacing();

					ui::center(rx, 90.0f, -1);

					ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10);
					ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
					ImGui::BeginChild("carousel", ImVec2(90, 20), true);
					{
						if (ImGui::Button(" < ", ImVec2(0, 17))) {
							if (m_Client.m_imageIndex - 1 >= 0) m_Client.m_imageIndex--;
						}
						ImGui::SameLine();
						ImGui::Text(std::format("{} of {}", m_Client.m_imageIndex + 1, m_Client.v_rawImageList.size()).c_str());
						ImGui::SameLine();
						ui::shift::right(20);
						if (ImGui::Button(" > ", ImVec2(0, 17)))
						{
							if (m_Client.m_imageIndex < ((int)m_Client.v_rawImageList.size() - 1))
								m_Client.m_imageIndex++;
						}
					}
					ImGui::EndChild();
					ImGui::PopStyleColor();

					ImGui::PopStyleVar(4);
				}

				ImGui::Spacing();

				parse_text(m_Client.m_resultsSchema.description.c_str());

				windowHeight = max(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y, windowHeight);
			}
			ImGui::EndChild();

			ImGui::SameLine();

			ImGui::BeginChild("right_side", ImVec2(rx, ry), false);
			{
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

				ImGui::BeginChild("actions_and_about", ImVec2(rx, 330), true);
				{
					ImGui::PopStyleColor();

					ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
					ImGui::Text("Actions");
					ImGui::PopFont();


					static ImU32 col = ImGui::GetColorU32(ImGuiCol_CheckMark);

					bool isInstalled = false;
					bool requiresUpdate = false;
	
					for (const auto item : m_Client.skinData)
					{
						try
						{
							if (this->m_itemSelectedSource.empty())
								continue;

							std::string val = this->m_itemSelectedSource.contains("source") ? this->m_itemSelectedSource["source"] : "";

							if (item.value("source", std::string()) == val) {

								//console.log(std::format("local installed: {}", this->m_itemSelectedSource["source"].get<std::string>()));
								isInstalled = true;

								if (item.value("version", "") != this->m_itemSelectedSource.value("version", ""))
									requiresUpdate = true;
							}
						}
						catch (const nlohmann::detail::exception& err) {
							console.err(std::format("error thrown checking if skin needs update on store page:\n{}", err.what()));
						}
						catch (const std::exception& err) {
							console.err(std::format("error thrown checking if skin needs update on store page:\n{}", err.what()));
						}
					}

					if (requiresUpdate || !isInstalled)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, col);
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);

						if (community::_installer->m_downloadInProgess)
							ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
						else
							ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);

						if (ImGui::Button((community::_installer->m_downloadInProgess ? community::_installer->m_downloadStatus.c_str() : requiresUpdate ? "Update" : "Download"), ImVec2(rx, 35))) {

							std::cout << m_Client.m_resultsSchema.id << std::endl;

							/*std::thread([this]() */{
								community::_installer->m_downloadInProgess = true;
								api->iterate_download_count(m_Client.m_resultsSchema.id);
								m_Client.m_resultsSchema.download_count++;

								std::cout << m_itemSelectedSource.dump(4) << std::endl;
								community::_installer->downloadTheme(m_itemSelectedSource);

								//this->createThemeSync(m_itemSelectedSource);

								m_Client.parseSkinData(false);
								community::_installer->m_downloadInProgess = false;
							}/*).detach();*/
						}

						ImGui::PopStyleColor(2);
						ImGui::PopFont();
					}
					else
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 0.0f, .5f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.0f, 0.0f, .5f));

						if (ImGui::Button("Uninstall", ImVec2(rx, 35))) {

							std::string disk_path = std::format("{}/{}", config.getSkinDir(), community::_installer->sanitizeDirectoryName(m_itemSelectedSource["name"].get<std::string>()));

							console.log(std::format("deleting skin {}", disk_path));

							if (std::filesystem::exists(disk_path)) {

								try {
									std::filesystem::remove_all(std::filesystem::path(disk_path));
								}
								catch (const std::exception& ex) {
									MsgBox(std::format("Couldn't remove the selected skin.\nError:{}", ex.what()).c_str(), "Non-fatal Error", MB_ICONERROR);
								}
							}

							Sleep(1000);
							m_Client.parseSkinData(false);
						}

						ImGui::PopStyleColor(2);
					}

					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));

					if (ImGui::Button("View Source", ImVec2(rx, 35))) {

						OpenURL(std::format("https://github.com/{}/{}", 
							m_Client.m_resultsSchema.gh_username, m_Client.m_resultsSchema.gh_repo).c_str())
					}

					ImGui::PopStyleColor(2);

					ImGui::Spacing();

					ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
					ImGui::Text("About");
					ImGui::PopFont();

					const auto render_about = [=](std::string title, std::string data)
						{
							ImGui::Text(title.c_str());

							ImGui::SameLine();
							ui::shift::x(-6);

							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.5, .5, .5, 1.0f));
							ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[3]);
							ImGui::Text(data.c_str());
							ImGui::PopFont();
							ImGui::PopStyleColor();
						};

					render_about("Downloads", std::to_string(m_Client.m_resultsSchema.download_count));
					render_about("ID", m_Client.m_resultsSchema.id);
					render_about("Date Added", m_Client.m_resultsSchema.date_added);
					render_about("Uploader", m_Client.m_resultsSchema.gh_username);

					ImGui::Spacing();
					ImGui::Spacing();

					ImGui::BeginChild("author", ImVec2(rx, 47), true);
					{
						ImGui::BeginChild("author_image", ImVec2(25, 25), false);
						{
							ImGui::Image(Window::iconsObj().github, ImVec2(25, 25));
						}
						ImGui::EndChild();
						ImGui::SameLine();
						ImGui::BeginChild("author_desc", ImVec2(rx, ry), false);
						{
							ui::shift::y(-3);
							ImGui::Text(m_Client.m_resultsSchema.gh_username.c_str());
							ui::shift::y(-5);
							ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
							ImGui::Text("https://github.com/%s", m_Client.m_resultsSchema.gh_username.c_str());
							ImGui::PopFont();
						}
						ImGui::EndChild();
					}
					ImGui::EndChild();
				}
				ImGui::EndChild();

				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

				ImGui::BeginChild("support_server", ImVec2(rx, 100), true);
				{

					ImGui::BeginChild("support_image", ImVec2(45, 45), false);
					{
						ImGui::Image(Window::iconsObj().support, ImVec2(45, 45));
					}
					ImGui::EndChild();

					ImGui::SameLine();

					ImGui::BeginChild("support_desc", ImVec2(rx, 45), false);
					{
						ui::shift::y(7);
						ImGui::Text(m_Client.m_resultsSchema.discord_name.c_str());
						ui::shift::y(-5);
						ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
						ImGui::Text("Support Server");
						ImGui::PopFont();
					}
					ImGui::EndChild();


					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.44f, 0.76f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.36f, 0.62f, 1.0f));

					if (ImGui::Button("Join Server", ImVec2(rx, ry)))
					{
						ShellExecute(NULL, "open", m_Client.m_resultsSchema.discord_link.c_str(), NULL, NULL, SW_SHOWNORMAL);
					}

					ImGui::PopStyleColor(2);
				}
				ImGui::EndChild();

				ImGui::BeginChild("tags-category", ImVec2(rx, 80), true);
				{
					std::vector<std::string> items = m_Client.m_resultsSchema.tags;

					ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
					ImGui::Text("Tags");
					ImGui::PopFont();
					ImGui::BeginChild("support_desc_container", ImVec2(rx, 30), false, ImGuiWindowFlags_HorizontalScrollbar);
					{
						ImGui::BeginChild("support_desc", ImVec2(rx, ry), false);
						{
							ImGui::PopStyleColor();

							ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15);

							for (int i = 0; i < (int)items.size(); i++)
							{
								ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
								const float width = ImGui::CalcTextSize(items[i].c_str()).x;

								ImGui::BeginChild(std::format("tag_{}", i).c_str(), ImVec2(width + 25.0f, ry), true);
								{
									ui::shift::x(5);
									ImGui::Text(items[i].c_str());
								}
								ImGui::EndChild();

								ImGui::PopFont();

								ImGui::SameLine();
								ui::shift::x(-6);
							}
							ImGui::PopStyleVar();
						}
						ImGui::EndChild();
					}
					ImGui::EndChild();
				}
				ImGui::EndChild();

				windowHeight = max(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y, windowHeight);
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();

		ImGui::PopStyleVar();
	}

	void millennium_tab() {
		//ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
		//{
		//	ui::center_text("About Millennium");
		//}
		//ImGui::PopFont();
		//ImGui::Spacing();

		//ui::center_text(std::format("Build: {}", __DATE__).c_str());
		//ui::center_text(std::format("Millennium API Version: {}", "v1").c_str());
		//ui::center_text(std::format("Client Version: {}", "1.0.0").c_str());
		//ui::center_text(std::format("Patcher Version: {}", m_ver).c_str());

		//ImGui::SetCursorPosY(ImGui::GetContentRegionMax().y - 100);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

		ImGui::BeginChild("support_server", ImVec2((rx * .5f) - 10, 100), true);
		{
			ImGui::BeginChild("support_image", ImVec2(45, 45), false);
			{
				ImGui::Image(Window::iconsObj().support, ImVec2(45, 45));
			}
			ImGui::EndChild();

			ImGui::SameLine();

			ImGui::BeginChild("support_desc", ImVec2(rx, 45), false);
			{
				ui::shift::y(7);
				ImGui::Text("Millennium");
				ui::shift::y(-5);
				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				ImGui::Text("Need Help? Found a bug?");
				ImGui::PopFont();
			}
			ImGui::EndChild();


			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.44f, 0.76f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.36f, 0.62f, 1.0f));

			if (ImGui::Button("Join Server", ImVec2(rx, ry)))
			{
				ShellExecute(NULL, "open", "https://discord.gg/MXMWEQKgJF", NULL, NULL, SW_SHOWNORMAL);
			}

			ImGui::PopStyleColor(2);
		}
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("kofi_child", ImVec2(rx, 100), true);
		{
			ImGui::BeginChild("kofi_image", ImVec2(45, 45), false);
			{
				ImGui::Image(Window::iconsObj().KofiLogo, ImVec2(45, 45));
			}
			ImGui::EndChild();

			ImGui::SameLine();

			ImGui::BeginChild("kofi_desc", ImVec2(rx, 45), false);
			{
				ui::shift::y(7);
				ImGui::Text("ShadowMonster");
				ui::shift::y(-5);
				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				ImGui::Text("Support me and the project on Kofi!");
				ImGui::PopFont();
			}
			ImGui::EndChild();


			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.44f, 0.76f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.36f, 0.62f, 1.0f));

			if (ImGui::Button("More details", ImVec2(rx, ry)))
			{
				ShellExecute(NULL, "open", "https://ko-fi.com/shadowmonster", NULL, NULL, SW_SHOWNORMAL);
			}

			ImGui::PopStyleColor(2);
		}
		ImGui::EndChild();

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Spacing();

		ui::center_text(std::format("Build: {}", __DATE__).c_str());
		ui::center_text(std::format("Millennium API Version: {}", "v1").c_str());
		ui::center_text(std::format("Client Version: {}", "1.0.0").c_str());
		ui::center_text(std::format("Patcher Version: {}", m_ver).c_str());


		ImGui::PopStyleColor();
	}

	void renderContentPanel()
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 10);
			ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

			if (ImGui::BeginChild("ChildContentPane", ImVec2(rx, ry), true))
			{
				switch (ui::current_tab_page)
				{
					case 1: store_panel();    break;
					case 2: library_panel();  break;
					case 3: settings_panel(); break;
					case 4: millennium_tab(); break;
				}
			}
			ImGui::EndChild();

			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
		}
		ImGui::PopStyleColor();
	}

	void renderSideBar()
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.00f));

		ImGui::BeginChild("LeftSideBar", ImVec2(rx, 40), false);
		{
			ImGui::PopStyleVar();
			ImGui::BeginChild("ChildFrameParent", ImVec2(rx, ry), true);
			{
				//ImGui::BeginChild("HeaderOptions", ImVec2(rx, 25));
				//{
				//	if (ImGui::IsWindowHovered())
				//	{
				//		ImGui::Image(Window::iconsObj().close, ImVec2(11, 11));
				//		if (ImGui::IsItemClicked()) {
				//			g_windowOpen = false;
				//			PostQuitMessage(0);
				//		}

				//		ImGui::SameLine();
				//		ui::shift::x(-6);

				//		//disabled maximize until i figure it out
				//		ImGui::Image(Window::iconsObj().greyed_out, ImVec2(11, 11));

				//		ImGui::SameLine();
				//		ui::shift::x(-6);

				//		ImGui::Image(Window::iconsObj().minimize, ImVec2(11, 11));
				//		if (ImGui::IsItemClicked()) {
				//			ShowWindow(Window::getHWND(), SW_MINIMIZE);
				//		}
				//	}
				//	else
				//	{
				//		ImGui::Image(Window::iconsObj().greyed_out, ImVec2(11, 11));

				//		ImGui::SameLine();
				//		ui::shift::x(-6);
				//		ImGui::Image(Window::iconsObj().greyed_out, ImVec2(11, 11));

				//		ImGui::SameLine();
				//		ui::shift::x(-6);
				//		ImGui::Image(Window::iconsObj().greyed_out, ImVec2(11, 11));
				//	}

				//	ImGui::SameLine();

				//	ImGui::BeginChild("HeaderDragArea", ImVec2(rx, ry));
				//	{
				//		if (ImGui::IsWindowHovered()) {
				//			g_headerHovered_1 = true;
				//		}
				//		else {
				//			g_headerHovered_1 = false;
				//		}
				//	}
				//	ImGui::EndChild();
				//}
				//ImGui::EndChild();

				//ImGui::SameLine();

				/*ImGui::BeginChild("ChildHeaderParent", ImVec2(rx, 70), true);
				{
					ImGui::BeginChild("ChildImageContainer", ImVec2(50, 50), false);
					{
						ImGui::Image(Window::iconsObj().planet, ImVec2(50, 50));
					}
					ImGui::EndChild();
					ImGui::SameLine();
					ImGui::BeginChild("ChildDescription", ImVec2(rx, 50), false);
					{
						ui::shift::y(7);
						ImGui::Text("Millennium");
						ui::shift::y(-5);
						ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
						{
							ImGui::Text("Steam Client Patcher.");
						}
						ImGui::PopFont();
					}
					ImGui::EndChild();
				}
				ImGui::EndChild();
				ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();*/

				//if (ImGui::BeginPopupContextItem("selectable_1")) 
				//{
				//	if (ImGui::MenuItem("Reload")) { api->retrieve_featured(); }
				//	ImGui::EndPopup();
				//}
				//if (ImGui::BeginPopupContextItem("selectable_2")) 
				//{
				//	if (ImGui::MenuItem("Reload")) { m_Client.parseSkinData(false); }
				//	ImGui::EndPopup();
				//}

				//ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				//{
				//	ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "MILLENNIUM");
				//}
				//ImGui::PopFont();

				if (ImGui::IsWindowHovered()) {
					g_headerHovered_1 = true;
				}
				else {
					g_headerHovered_1 = false;
				}


				//ImGui::ImageButton(Window::iconsObj().planetLogo20, ImVec2(18, 18));



				//ImGui::SameLine();
				//ui::shift::x(-4);
				//ImGui::Text(std::format("Millennium v{} �", m_ver).c_str());
				//ImGui::SameLine();

				//listButton(" Millennium ", 4);
				//if (ImGui::IsItemHovered()) g_headerHovered_1 = false;
				//ImGui::SameLine();
				//ui::shift::x(-4);

				listButton(" Library ", 2);
				if (ImGui::IsItemHovered()) g_headerHovered_1 = false;
				ImGui::SameLine();
				ui::shift::x(-4);

				static ImU32 col = ImGui::GetColorU32(ImGuiCol_CheckMark);

				if (ImGui::Button("Community")) {
					steam_js_context SharedJsContext;

					std::string url = "https://bettersteam.web.app/themes";
					std::string loadUrl = std::format("SteamUIStore.Navigate('/browser', MainWindowBrowserManager.LoadURL('{}'));", url);

					SharedJsContext.exec_command(loadUrl);
				}
				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

				if (ImGui::IsItemHovered()) g_headerHovered_1 = false;
				ImGui::SameLine();
				ui::shift::x(-4);
				//ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				//{
				//	ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "GENERAL");
				//}
				//ImGui::PopFont();

				listButton(" Settings ", 3);
				if (ImGui::IsItemHovered()) g_headerHovered_1 = false;
				//ImGui::SameLine();
				//ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

				ImGui::SameLine();

				ui::shift::right(25);

				if (ImGui::ImageButton(Window::iconsObj().xbtn, ImVec2(16, 16))) {
					g_windowOpen = false;
					PostQuitMessage(0);
				}
				if (ImGui::IsItemHovered()) g_headerHovered_1 = false;

				//if (ImGui::Selectable(" About")) {
				//	ImGui::OpenPopup(" About Millennium");
				//}
				//ImGui::PopStyleColor();

				//ImGui::InsertNotification({ ImGuiToastType_Error, 3000, "Hello World! This is an error!" });

				//if (ImGui::Button("Unload Millennium")) {
				//	FreeConsole();
				//	MsgBox("Unloading Millennium from Steam", "Error", 0);
				//	
				//	CreateThread(nullptr, NULL, (LPTHREAD_START_ROUTINE)FreeLibrary, hCurrentModule, NULL, nullptr);

				//	Application::Destroy();
				//	threadContainer::getInstance().killAllThreads(0);
				//}

				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

				ui::center_modal(ImVec2(ImGui::GetMainViewport()->WorkSize.x / 2.3f, ImGui::GetMainViewport()->WorkSize.y / 2.4f));

				ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6);

				if (ImGui::BeginPopupModal(" About Millennium", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar))
				{
					ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));

					ImGui::BeginChild("HeaderOptions", ImVec2(rx, 25));
					{
						if (ImGui::IsWindowHovered())
						{
							ImGui::Image(Window::iconsObj().close, ImVec2(11, 11));
							if (ImGui::IsItemClicked()) {
								ImGui::CloseCurrentPopup();
							}
						}
						else
						{
							ImGui::Image(Window::iconsObj().greyed_out, ImVec2(11, 11));
						}

						ImGui::SameLine();

						ImGui::BeginChild("HeaderDragArea", ImVec2(rx, ry));
						{
							ImGui::PopStyleColor();

							if (ImGui::IsWindowHovered()) {
								g_headerHovered_1 = true;
							}
							else {
								g_headerHovered_1 = false;
							}
						}
						ImGui::EndChild();
					}
					ImGui::EndChild();

					ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
					{
						ui::center_text("About Millennium");
					}
					ImGui::PopFont();
					ImGui::Spacing();

					ui::center_text(std::format("Build: {}", __DATE__).c_str());
					ui::center_text(std::format("Millennium API Version: {}", "v1").c_str());
					ui::center_text(std::format("Client Version: {}", "1.0.0").c_str());
					ui::center_text(std::format("Patcher Version: {}", m_ver).c_str());

					ImGui::SetCursorPosY(ImGui::GetContentRegionMax().y - 100);
					ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

					ImGui::BeginChild("support_server", ImVec2(rx, 100), true);
					{
						ImGui::BeginChild("support_image", ImVec2(45, 45), false);
						{
							ImGui::Image(Window::iconsObj().support, ImVec2(45, 45));
						}
						ImGui::EndChild();

						ImGui::SameLine();

						ImGui::BeginChild("support_desc", ImVec2(rx, 45), false);
						{
							ui::shift::y(7);
							ImGui::Text("Millennium");
							ui::shift::y(-5);
							ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
							ImGui::Text("Need Help? Found a bug?");
							ImGui::PopFont();
						}
						ImGui::EndChild();


						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.44f, 0.76f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.36f, 0.62f, 1.0f));

						if (ImGui::Button("Join Server", ImVec2(rx, ry)))
						{
							ShellExecute(NULL, "open", "https://discord.gg/MXMWEQKgJF", NULL, NULL, SW_SHOWNORMAL);
						}

						ImGui::PopStyleColor(2);
					}
					ImGui::EndChild();

					ImGui::PopStyleColor();

					ImGui::EndPopup();
				}

				ImGui::PopStyleVar();

				//ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				//{
				//	std::string v_buildInfo = std::format("Build: {}\nMillennium Patcher Version: {}", __DATE__, m_ver);
				//	ImGui::SetCursorPosY(ImGui::GetContentRegionMax().y - ImGui::CalcTextSize(v_buildInfo.c_str()).y);
				//	ImGui::Text(v_buildInfo.c_str());
				//}
				//ImGui::PopFont();
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();

		ImGui::PopStyleColor();
	}
} RendererProc;

void handleFileDrop()
{
	if (g_fileDropQueried) {
		ui::current_tab_page = 2;
	}
}

void handleEdit()
{
	//std::cout << RendererProc.m_editObj.dump(4) << std::endl;

	const std::string name = RendererProc.m_editObj.empty() ? 
		"Nothing Selected" : 
		(RendererProc.m_editObj.contains("name") ? RendererProc.m_editObj["name"] : 
			RendererProc.m_editObj.contains("native_name") ? RendererProc.m_editObj["native_name"] : "Null");

	if (RendererProc.m_editMenuOpen)
	{
		ImGui::OpenPopup(std::format(" Settings for {}", name).c_str());
	}

	//ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6);

	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.2f, 0.2f, 1.0f));
	//ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));

	ImGui::SetNextWindowSize(ImVec2(450, 550), ImGuiCond_Once);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
	if (ImGui::BeginPopupModal(std::format(" Settings for {}", name).c_str(), &RendererProc.m_editMenuOpen, ImGuiWindowFlags_NoCollapse))
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
		ImGui::BeginChild("###ConfigContainer", ImVec2(rx, ry - 32), false);
		{
			const bool hasConfiguration = RendererProc.m_editObj.contains("Configuration");
			const bool hasColors = !RendererProc.colorList.empty();

			if (!hasConfiguration && !hasColors) {
				ImGui::Text("No Settings Available");
			}

			if (hasConfiguration)
			{
				ImGui::Spacing();
				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);

				ImGui::Text(" Configuration Settings");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::PopFont();

				for (auto& setting : RendererProc.m_editObj["Configuration"])
				{
					if (!setting.contains("Name"))
						continue;
					if (!setting.contains("Type"))
						continue;

					const std::string name = setting["Name"];
					const std::string toolTip = setting.value("ToolTip", std::string());
					const std::string type = setting["Type"];

					if (type == "CheckBox") {
						bool value = setting.value("Value", false);

						ui::render_setting(
							name.c_str(), toolTip.c_str(),
							value, false,
							[&]() { setting["Value"] = value; }
						);
					}
				}
			}

			if (hasColors)
			{
				ImGui::Spacing();
				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
				ImGui::Text(" Color Settings:");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::PopFont();

				if (ImGui::Button("Reset Colors"))
				{
					auto& obj = RendererProc.m_editObj;

					if (obj.contains("GlobalsColors") && obj.is_object())
					{
						for (auto& color : obj["GlobalsColors"])
						{
							if (!color.contains("HexColorCode") || !color.contains("OriginalColorCode"))
							{
								console.err("Couldn't reset colors. 'HexColorCode' or 'OriginalColorCode' doesn't exist");
								continue;
							}

							color["HexColorCode"] = color["OriginalColorCode"];
						}
					}
					else {
						console.log("Theme doesn't have GlobalColors");
					}

					config.setThemeData(obj);
					RendererProc.openPopupMenu(obj);
				}

				auto& colors = RendererProc.colorList;

				for (size_t i = 0; i < RendererProc.colorList.size(); i++)
				{
					if (ImGui::Button("Reset"))
					{
						auto& obj = RendererProc.m_editObj;

						if (obj.contains("GlobalsColors") && obj.is_object())
						{
							auto& global = obj["GlobalsColors"][i];

							console.log(std::format("global colors: {}", global.dump(4)));
							console.log(std::format("color name: {}", colors[i].name));

							if (global["ColorName"] == colors[i].name) {

								console.log(std::format("Color match. Setting color {} from {} to {}",
									global["ColorName"].get<std::string>(),
									global["HexColorCode"].get<std::string>(),
									global["OriginalColorCode"].get<std::string>()));

								global["HexColorCode"] = global["OriginalColorCode"];
							}
							else {
								MsgBox(std::format("Couldn't Set color at index {} because the buffer was mismatching.", i).c_str(), "Error", MB_ICONERROR);
							}

							config.setThemeData(obj);
							m_Client.parseSkinData(false);

							RendererProc.openPopupMenu(obj);

							themeConfig::updateEvents::getInstance().triggerUpdate();

							steam_js_context SharedJsContext;
							SharedJsContext.reload();
						}
						else {
							console.log("Theme doesn't have GlobalColors");
						}
					}

					ImGui::SameLine();
					ui::shift::x(-8);

					ImGui::PushItemWidth(90);
					ImGui::ColorEdit3(std::format("##colorpicker_{}", i).c_str(), &colors[i].color.x, ImGuiColorEditFlags_DisplayHex);
					ImGui::PopItemWidth();

					ImGui::SameLine();
					ui::shift::x(-4);

					ImGui::Text(std::format("{} [{}]", colors[i].comment, colors[i].name).c_str());

				}
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();

		if (ImGui::Button("Save and Update", ImVec2(rx, 24)))
		{
			const auto json = RendererProc.m_editObj;

			nlohmann::json buffer = config.getThemeData(true);

			console.log(buffer.dump(4));

			if (buffer.contains("config_fail") && buffer["config_fail"]) {
				MsgBox("Unable to save and update config. Millennium couldn't get theme data", "Error", MB_ICONERROR);
			}
			else {

				if (buffer.contains("Configuration") && json.contains("Configuration"))
				{
					buffer["Configuration"] = json["Configuration"];
					config.setThemeData(buffer);

					auto& colors = RendererProc.colorList;
					auto& obj = RendererProc.m_editObj;

					if (obj.contains("GlobalsColors") && obj.is_object())
					{
						for (size_t i = 0; i < obj["GlobalsColors"].size(); i++)
						{
							auto& global = obj["GlobalsColors"][i];

							if (!global.contains("HexColorCode"))
							{
								console.err("Couldn't reset colors. 'HexColorCode' or 'OriginalColorCode' doesn't exist");
								continue;
							}

							if (global["ColorName"] == colors[i].name) {
								global["HexColorCode"] = "#" + colors::ImVec4ToHex(colors[i].color);
							}
							else {
								console.err(std::format("Color at index {} was a mismatch", i));
							}
						}

						config.setThemeData(obj);
						m_Client.parseSkinData(false);

						themeConfig::updateEvents::getInstance().triggerUpdate();

						steam_js_context SharedJsContext;
						SharedJsContext.reload();
					}
					else {
						console.log("Theme doesn't have GlobalColors");
					}

					m_Client.parseSkinData(false);
				}
				else {
					console.log("json buffer or editing object doesn't have a 'Configuration' key");
				}
			}
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();

	ImGui::PopStyleColor();
	//ImGui::PopStyleVar();
}


void init_main_window()
{
	g_windowOpen = true;

	//get_update_list();
	// GET information on initial load
	const auto initCallback = ([=](void) -> void {
		//m_Client.parseSkinData(true);

		//get_update_list();
		//api->retrieve_featured();
		api->retrieve_featured();

		std::thread([&]() {
			m_Client.parseSkinData(true);
		}).detach();
	});

	// window callback 
	const auto wndProcCallback = ([=](void) -> void {

		handleFileDrop();
		handleEdit();

		RendererProc.renderSideBar();

		//ImGui::SameLine(); 
		ui::shift::y(-8);
		RendererProc.renderContentPanel();
	});

	//std::thread([&]() {
	//	themeConfig::watchPath(config.getSkinDir(), []() {
	//		try {
	//			m_Client.parseSkinData(false, false, "");
	//		}
	//		catch (std::exception& ex) {
	//			console.log(ex.what());
	//		}
	//	});
	//}).detach();

	Window::setTitle((char*)"Millennium.Steam.Client");
	Window::setDimensions(ImVec2({ 450, 500 }));

	Application::Create(wndProcCallback, initCallback);
}
