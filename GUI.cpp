#include "GUI.h"
#include <Geode/Geode.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "WindowAction.h"
#include "../Settings.hpp"

#include <fstream>
#include <sstream>

#include <imgui-cocos.hpp>
#include "ConstData.h"

#include "Common.h"
#include "Hacks/SafeMode.h"

#include "Blur.h"

using namespace geode::prelude;

class $modify(CCKeyboardDispatcher)
{
	bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool arr)
	{
		if (!arr)
		{
			int menuKey = Settings::get<int>("menu/togglekey", ImGuiKey_Tab);

			if (menuKey == 0x09)
				menuKey = ImGuiKey_Tab;

#if !defined(__ANDROID__)
			if (down && (ConvertKeyEnum(key) == menuKey))
			{
				GUI::toggle();
				return true;
			}
#endif

			if (down)
			{
				for (GUI::Shortcut& s : GUI::shortcuts)
				{
					if (ConvertKeyEnum(key) == s.key)
					{
						GUI::currentShortcut = s.name;
						GUI::shortcutLoop = true;
						GUI::draw();
						GUI::shortcutLoop = false;

						Common::updateCheating();
						SafeMode::updateAuto();
					}
				}
			}

			if (!ImGuiCocos::get().isInitialized())
				return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, arr);

			if (const auto imKey = ConvertKeyEnum(key); imKey != ImGuiKey_None)
				ImGui::GetIO().AddKeyEvent(imKey, down);
		}

		return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, arr);
	}
};

class $modify(MenuLayer)
{
	bool init()
	{
		if (!GUI::hasLateInit)
		{
			GUI::lateInit();
			GUI::canToggle = true;
		}
		GUI::hasLateInit = true;

		return MenuLayer::init();
	}
};

ImVec2 GUI::getJsonPosition(const std::string& name)
{
	if (!windowPositions.contains(name))
	{
		windowPositions[name]["x"] = .0f;
		windowPositions[name]["y"] = .0f;
	}

	auto winSize = CCDirector::sharedDirector()->getOpenGLView()->getViewPortRect();

	if (windowPositions[name]["x"] > 1 || windowPositions[name]["y"] > 1)
		winSize.size = cocos2d::CCSize(1.f, 1.f);

	return {windowPositions[name]["x"].get<float>() * winSize.size.width, windowPositions[name]["y"].get<float>() * winSize.size.height};
}

void GUI::setJsonPosition(const std::string& name, ImVec2 pos)
{
	windowPositions[name]["x"] = pos.x;
	windowPositions[name]["y"] = pos.y;
}

ImVec2 GUI::getJsonSize(const std::string& name, ImVec2 defaultSize)
{
	if (!windowPositions.contains(name) || !windowPositions[name].contains("w"))
	{
		windowPositions[name]["w"] = defaultSize.x;
		windowPositions[name]["h"] = defaultSize.y;
	}

	auto winSize = CCDirector::sharedDirector()->getOpenGLView()->getViewPortRect();

	if (windowPositions[name]["w"] > 1 || windowPositions[name]["h"] > 1)
		winSize.size = cocos2d::CCSize(1.f, 1.f);

	return {windowPositions[name]["w"].get<float>() * winSize.size.width, windowPositions[name]["h"].get<float>() * winSize.size.height};
}

void GUI::setJsonSize(const std::string& name, ImVec2 size)
{
	windowPositions[name]["w"] = size.x;
	windowPositions[name]["h"] = size.y;
}

void GUI::init()
{
	hasLateInit = false;
	uiSizeFactor = CCDirector::sharedDirector()->getOpenGLView()->getFrameSize().width / 1920.0f;
	Blur::compiled = false;
	
	windowPositions = json::object();

	ghc::filesystem::path defaultStylePath = Mod::get()->getSaveDir() / "styles" / "default.style";
	ghc::filesystem::copy(Mod::get()->getResourcesDir() / "Style.style", defaultStylePath, ghc::filesystem::copy_options::overwrite_existing);

	ghc::filesystem::path defaultFontPath = Mod::get()->getSaveDir() / "fonts" / "Roboto-Regular.ttf";
	ghc::filesystem::copy(Mod::get()->getResourcesDir() / "Roboto-Regular.ttf", defaultFontPath, ghc::filesystem::copy_options::overwrite_existing);

	styleCombo = DirectoryCombo("Style", Mod::get()->getSaveDir() / "styles", "menu/style_path");
	fontCombo = DirectoryCombo("Font", Mod::get()->getSaveDir() / "fonts", "menu/font_path");

	std::string fontPath = Settings::get<std::string>("menu/font_path", string::wideToUtf8(defaultFontPath.wstring()));
	
	if(fontPath.empty() || !ghc::filesystem::exists(fontPath))
		fontPath = string::wideToUtf8(defaultFontPath.wstring());

	std::string stylePath = Settings::get<std::string>("menu/style_path", string::wideToUtf8(defaultStylePath.wstring()));
	if(stylePath.empty() || !ghc::filesystem::exists(stylePath))
		stylePath = string::wideToUtf8(defaultStylePath.wstring());
	
	GUI::loadStyle(stylePath);
	setFont(fontPath);

	shortcuts.clear();
	shadowTexture = cocos2d::CCTextureCache::get()->addImage(string::wideToUtf8((Mod::get()->getResourcesDir() / "shadow.png").wstring()).c_str(), true);
	load();
}

void GUI::setLateInit(const std::function<void()>& func)
{
	lateInit = func;
}

void GUI::draw()
{
	uiSizeFactor = CCDirector::sharedDirector()->getOpenGLView()->getFrameSize().width / 1920.0f;
	uiSizeFactor *= Settings::get<float>("menu/ui_scale", 1.f);

	if (!isVisible && !shortcutLoop)
		return;

	ImGui::GetStyle() = loadedStyle;

	ImGui::GetStyle().ScaleAllSizes(uiSizeFactor);

	for (WindowAction* ac : windowActions)
		ac->step(ImGui::GetIO().DeltaTime);

	for (Window& w : windows)
	{
		if(w.enabled)
			w.draw();
	}

	float transitionDuration = Settings::get<float>("menu/transition_duration", 0.35f);

	hideTimer += ImGui::GetIO().DeltaTime;
	if (hideTimer > transitionDuration)
		isVisible = false;
}

void GUI::toggle()
{
	if (!canToggle)
		return;

	Common::updateCheating();
	SafeMode::updateAuto();
	Blur::compileBlurShader();

	isVisible = true;
	toggled = !toggled;
																											//isPaused
#if !defined(__ANDROID__)
	cocos2d::CCEGLView::sharedOpenGLView()->showCursor(toggled || !PlayLayer::get() || (PlayLayer::get() && MBO(bool, PlayLayer::get(), 0x2F17)) || (PlayLayer::get() && PlayLayer::get()->getChildByID("EndLevelLayer")));
#endif

	// 🔥🔥🔥🔥
	hideTimer = toggled ? -FLT_MAX : 0;

	if (!toggled)
		save();

	for (WindowAction* ac : windowActions)
		delete ac;

	windowActions.clear();

	int direction = std::rand() % 8 + 1;

	for (Window& w : windows)
	{
		uint8_t animationType = (w.name.length() + direction) % 8;

		ImVec2 dir;

		switch (animationType)
		{
		case 0:
			dir = {1400, 1400};
			break;
		case 1:
			dir = {1400, -1400};
			break;
		case 2:
			dir = {-1400, 1400};
			break;
		case 3:
			dir = {-1400, -1400};
			break;
		case 4:
			dir = {-2400, 0};
			break;
		case 5:
			dir = {2400, 0};
			break;
		case 6:
			dir = {0, -2400};
			break;
		case 7:
			dir = {0, 2400};
			break;
		}

		float transitionDuration = Settings::get<float>("menu/transition_duration", 0.35f);

		WindowAction* action = WindowAction::create(
			transitionDuration, &w, toggled ? w.position : ImVec2(w.position.x + dir.x, w.position.y + dir.y));
		windowActions.push_back(action);
	}
}

void GUI::addWindow(Window window)
{
	if (windowPositions.contains(window.name))
	{
		// Saved window geometry is stored relative to the current viewport.
		window.position = getJsonPosition(window.name);
		window.size = getJsonSize(window.name, window.size);
	}
	else
	{
		// The original defaults were authored for a 1920px-wide desktop.
		// Scale only first-run defaults so the original windows remain reachable
		// on narrower Android screens. Saved positions are already viewport-relative.
		window.position = {
			window.position.x * uiSizeFactor,
			window.position.y * uiSizeFactor
		};
		window.size = {
			window.size.x * uiSizeFactor,
			window.size.y * uiSizeFactor
		};
	}

	int direction = std::rand() % 4 + 1;

	uint8_t animationType = (window.name.length() + direction) % 4;

	switch (animationType)
	{
	case 0:
		window.renderPosition = {1400, 1400};
		break;
	case 1:
		window.renderPosition = {1400, -1400};
		break;
	case 2:
		window.renderPosition = {-1400, 1400};
		break;
	case 3:
		window.renderPosition = {-1400, -1400};
		break;
	}

	windows.push_back(window);
	windowReferences[window.name] = &windows.back();
}

void GUI::save()
{
	for (Window& w : windows)
	{
		windowPositions[w.name]["x"] = (float)w.position.x / (float)ImGui::GetIO().DisplaySize.x;
		windowPositions[w.name]["y"] = (float)w.position.y / (float)ImGui::GetIO().DisplaySize.y;
		windowPositions[w.name]["w"] = (float)w.size.x / (float)ImGui::GetIO().DisplaySize.x;
		windowPositions[w.name]["h"] = (float)w.size.y / (float)ImGui::GetIO().DisplaySize.y;
	}

	std::ofstream f(Mod::get()->getSaveDir() / "windows.json");
	f << windowPositions.dump(4);
	f.close();

	f.open(Mod::get()->getSaveDir() / "shortcuts.json");
	json shortcutArray = json::array();

	for (Shortcut& s : shortcuts)
	{
		json shortcutObject = json::object();
		shortcutObject["name"] = s.name;
		shortcutObject["key"] = s.key;
		shortcutArray.push_back(shortcutObject);
	}

	f << shortcutArray.dump(4);
	f.close();

	Mod::get()->saveData();
}

void GUI::load()
{
	std::ifstream f(Mod::get()->getSaveDir() / "windows.json");

	if (!f)
	{
		f.close();

		f.open(Mod::get()->getResourcesDir() / "default_windows.json");
	}

	if (f)
	{
		std::stringstream buffer;
		buffer << f.rdbuf();
		try
		{
			windowPositions = json::parse(buffer.str());
		}
		catch(json::parse_error& ex)
		{
			log::error("{}", ex.what());
			windowPositions = json::object();
		}
		buffer.clear();
	}
	f.close();

	f.open(Mod::get()->getSaveDir() / "shortcuts.json");
	if (f)
	{
		std::stringstream buffer;
		buffer << f.rdbuf();
		json shortcutArray = json::object();
		try
		{
			shortcutArray = json::parse(buffer.str());
		}
		catch(json::parse_error& ex)
		{
			log::error("{}", ex.what());
			shortcutArray = json::object();
		}
		buffer.clear();

		for (json shortcutObject : shortcutArray)
		{
			Shortcut s(shortcutObject["key"], shortcutObject["name"]);
			GUI::shortcuts.push_back(s);
		}
	}
	f.close();
}

void GUI::resetDefault()
{
	std::ifstream f(Mod::get()->getResourcesDir() / "default_windows.json");

	if (f)
	{
		std::stringstream buffer;
		buffer << f.rdbuf();
		try
		{
			windowPositions = json::parse(buffer.str());
		}
		catch(json::parse_error& ex)
		{
			log::error("{}", ex.what());
			windowPositions = json::object();
		}
		buffer.clear();
	}
	f.close();

	for(Window& window : windows)
	{
		window.position = getJsonPosition(window.name);
		window.renderPosition = window.position;
		window.size = getJsonSize(window.name, window.size);
	}
}

void GUI::saveStyle(const ghc::filesystem::path& name)
{
	std::ofstream styleFile(name, std::ios::binary);
	if (styleFile)
	{
		styleFile.write((const char*)&loadedStyle, sizeof(ImGuiStyle));
		styleFile.write((const char*)&fontScale, sizeof(float));
	}
	styleFile.close();
}

void GUI::loadStyle(const ghc::filesystem::path& name)
{
	std::ifstream styleFile(name, std::ios::binary);
	if (styleFile)
	{
		styleFile.seekg(0, std::ios::end);
		int size = styleFile.tellg();
		styleFile.seekg(0, std::ios::beg);

		styleFile.read((char*)&loadedStyle, sizeof(ImGuiStyle));

		if(size > sizeof(ImGuiStyle))
			styleFile.read((char*)&fontScale, sizeof(float));
		else
			fontScale = 16.f;
	}
	
	styleFile.close();
}

void GUI::setFont(const ghc::filesystem::path& font)
{
	ImFont* fnt = ImGui::GetIO().Fonts->AddFontFromFileTTF(string::wideToUtf8(font.wstring()).c_str(), fontScale * uiSizeFactor);
	ImGui::GetIO().FontDefault = fnt;
	ImGuiCocos::get().reloadFontTexture();
}

void GUI::setStyle()
{
	float r = Settings::get<float>("menu/window/color/r", 1.f);
	float g = Settings::get<float>("menu/window/color/g", 0.f);
	float b = Settings::get<float>("menu/window/color/b", 0.f);

	if (Settings::get<bool>("menu/window/rainbow/enabled"))
	{
		ImGui::ColorConvertHSVtoRGB(
			ImGui::GetTime() * Settings::get<float>("menu/window/rainbow/speed", .4f),
			Settings::get<float>("menu/window/rainbow/brightness", .8f),
			Settings::get<float>("menu/window/rainbow/brightness"),
			r, g, b
		);

		GUI::loadedStyle.Colors[ImGuiCol_Border] = { r, g, b, 1 };
		GUI::loadedStyle.Colors[ImGuiCol_TitleBg] = { r, g, b, 1 };
		GUI::loadedStyle.Colors[ImGuiCol_TitleBgCollapsed] = { r, g, b, 1 };
		GUI::loadedStyle.Colors[ImGuiCol_TitleBgActive] = { r, g, b, 1 };

		GUI::loadedStyle.Colors[ImGuiCol_Tab] = { r * .85f, g * .85f, b * .85f, 1 };
		GUI::loadedStyle.Colors[ImGuiCol_TabActive] = { r * .85f, g * .85f, b * .85f, 1 };
		GUI::loadedStyle.Colors[ImGuiCol_TabHovered] = { r * .70f, g * .70f, b * .70f, 1 };
		GUI::loadedStyle.Colors[ImGuiCol_TabUnfocused] = { r * .60f, g * .60f, b * .60f, 1 };
		GUI::loadedStyle.Colors[ImGuiCol_TabUnfocusedActive] = { r * .60f, g * .60f, b * .60f, 1 };
	}
	else
	{
		GUI::loadedStyle.Colors[ImGuiCol_TitleBg] = { r, g, b, 1.f };
		GUI::loadedStyle.Colors[ImGuiCol_TitleBgActive] = { r, g, b, 1.f };
		GUI::loadedStyle.Colors[ImGuiCol_TitleBgCollapsed] = { r, g, b, 1.f };
	}
}

bool GUI::shouldRender()
{
	return isVisible && !shortcutLoop;
}
