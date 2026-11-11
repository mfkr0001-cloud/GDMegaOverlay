
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <filesystem>
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
#include "hacks/SafeMode.h"

#include "Blur.h"

using namespace geode::prelude;

class $modify(CCKeyboardDispatcher)
{
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool arr, double time)
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
                return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, false, time);

            if (const auto imKey = ConvertKeyEnum(key); imKey != ImGuiKey_None)
                ImGui::GetIO().AddKeyEvent(imKey, down);
        }

        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, false, time);
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
    if (!windowPositions.contains(name) || !windowPositions[name].is_object())
        windowPositions[name] = json::object();

    auto& entry = windowPositions[name];
    if (!entry.contains("x")) entry["x"] = 0.0f;
    if (!entry.contains("y")) entry["y"] = 0.0f;

    const auto viewport = CCDirector::sharedDirector()->getOpenGLView()->getViewPortRect();
    const float x = entry.value("x", 0.0f);
    const float y = entry.value("y", 0.0f);
    // Older builds stored absolute pixels; current files store viewport fractions.
    return {
        (x >= -1.0f && x <= 1.0f) ? x * viewport.size.width : x,
        (y >= -1.0f && y <= 1.0f) ? y * viewport.size.height : y
    };
}

void GUI::setJsonPosition(const std::string& name, ImVec2 pos)
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    windowPositions[name]["x"] = display.x > 0 ? pos.x / display.x : pos.x;
    windowPositions[name]["y"] = display.y > 0 ? pos.y / display.y : pos.y;
}

ImVec2 GUI::getJsonSize(const std::string& name, ImVec2 defaultSize)
{
    if (!windowPositions.contains(name) || !windowPositions[name].is_object() ||
        !windowPositions[name].contains("w") || !windowPositions[name].contains("h"))
    {
        windowPositions[name]["w"] = defaultSize.x;
        windowPositions[name]["h"] = defaultSize.y;
    }

    const auto viewport = CCDirector::sharedDirector()->getOpenGLView()->getViewPortRect();
    const float w = windowPositions[name].value("w", defaultSize.x);
    const float h = windowPositions[name].value("h", defaultSize.y);
    return {
        (w > 0.0f && w <= 1.0f) ? w * viewport.size.width : w,
        (h > 0.0f && h <= 1.0f) ? h * viewport.size.height : h
    };
}

void GUI::setJsonSize(const std::string& name, ImVec2 size)
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    windowPositions[name]["w"] = display.x > 0 ? size.x / display.x : size.x;
    windowPositions[name]["h"] = display.y > 0 ? size.y / display.y : size.y;
}

void GUI::init()
{
    hasLateInit = false;
    uiSizeFactor = CCDirector::sharedDirector()->getOpenGLView()->getFrameSize().width / 1920.0f;
    uiSizeFactor *= Settings::get<float>("menu/ui_scale", 1.f);
#if defined(__ANDROID__)
    // Apply the Android scale before fonts and default window sizes are created.
    // This keeps text, widgets, tabs and window geometry in the same scale.
    uiSizeFactor *= 0.50f;
#endif
    Blur::compiled = false;

    windowPositions = json::object();

    std::filesystem::path defaultStylePath = Mod::get()->getSaveDir() / "styles" / "default.style";
    std::filesystem::copy(
        Mod::get()->getResourcesDir() / "Style.style",
        defaultStylePath,
        std::filesystem::copy_options::overwrite_existing
    );

    std::filesystem::path defaultFontPath = Mod::get()->getSaveDir() / "fonts" / "Roboto-Regular.ttf";
    std::filesystem::copy(
        Mod::get()->getResourcesDir() / "Roboto-Regular.ttf",
        defaultFontPath,
        std::filesystem::copy_options::overwrite_existing
    );

    styleCombo = DirectoryCombo("Style", Mod::get()->getSaveDir() / "styles", "menu/style_path");
    fontCombo = DirectoryCombo("Font", Mod::get()->getSaveDir() / "fonts", "menu/font_path");

    std::string fontPath = Settings::get<std::string>("menu/font_path", defaultFontPath.string());

    if (fontPath.empty() || !std::filesystem::exists(fontPath))
        fontPath = defaultFontPath.string();

    std::string stylePath = Settings::get<std::string>("menu/style_path", defaultStylePath.string());

    if (stylePath.empty() || !std::filesystem::exists(stylePath))
        stylePath = defaultStylePath.string();

    GUI::loadStyle(stylePath);
    setFont(fontPath);

    shortcuts.clear();
    shadowTexture = cocos2d::CCTextureCache::get()->addImage(
        (Mod::get()->getResourcesDir() / "shadow.png").string().c_str(),
        true
    );

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

#if defined(__ANDROID__)
    // Keep draw-time widget constraints in sync with the font and window scale
    // selected in GUI::init().
    uiSizeFactor *= 0.50f;
#endif

    if (!isVisible && !shortcutLoop)
        return;

    ImGui::GetStyle() = loadedStyle;
    ImGui::GetStyle().ScaleAllSizes(uiSizeFactor);

    for (WindowAction* ac : windowActions)
        ac->step(ImGui::GetIO().DeltaTime);

    for (Window& w : windows)
    {
        if (w.enabled)
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

#if !defined(__ANDROID__)
    cocos2d::CCEGLView::sharedOpenGLView()->showCursor(
        toggled ||
        !PlayLayer::get() ||
        (PlayLayer::get() && MBO(bool, PlayLayer::get(), 0x2F17)) ||
        (PlayLayer::get() && PlayLayer::get()->getChildByID("EndLevelLayer"))
    );
#endif

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
            transitionDuration,
            &w,
            toggled ? w.position : ImVec2(w.position.x + dir.x, w.position.y + dir.y)
        );

        windowActions.push_back(action);
    }
}

void GUI::addWindow(Window window)
{
    if (windowPositions.contains(window.name))
    {
        window.position = getJsonPosition(window.name);
        window.size = getJsonSize(window.name, window.size);
        window.fontScale = windowPositions[window.name].value("fontScale", 1.0f);
    }
    else
    {
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
        setJsonPosition(w.name, w.position);
        setJsonSize(w.name, w.size);
        windowPositions[w.name]["fontScale"] = w.fontScale;
    }

    std::filesystem::create_directories(Mod::get()->getSaveDir());
    std::ofstream f(Mod::get()->getSaveDir() / "windows.json");
    if (f) f << windowPositions.dump(4);
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
        catch (json::parse_error& ex)
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
        catch (json::parse_error& ex)
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
        catch (json::parse_error& ex)
        {
            log::error("{}", ex.what());
            windowPositions = json::object();
        }

        buffer.clear();
    }

    f.close();

    for (Window& window : windows)
    {
        window.position = getJsonPosition(window.name);
        window.renderPosition = window.position;
        window.size = getJsonSize(window.name, window.size);
        window.fontScale = windowPositions[window.name].value("fontScale", 1.0f);
        window.geometryDirty = true;
    }
    save();
}

void GUI::saveStyle(const std::filesystem::path& name)
{
    std::ofstream styleFile(name, std::ios::binary);

    if (styleFile)
    {
        styleFile.write((const char*)&loadedStyle, sizeof(ImGuiStyle));
        styleFile.write((const char*)&fontScale, sizeof(float));
    }

    styleFile.close();
}

void GUI::loadStyle(const std::filesystem::path& name)
{
    std::ifstream styleFile(name, std::ios::binary);

    if (styleFile)
    {
        styleFile.seekg(0, std::ios::end);
        int size = styleFile.tellg();
        styleFile.seekg(0, std::ios::beg);

        styleFile.read((char*)&loadedStyle, sizeof(ImGuiStyle));

        if (size > sizeof(ImGuiStyle))
            styleFile.read((char*)&fontScale, sizeof(float));
        else
            fontScale = 16.f;
    }

    styleFile.close();
}

void GUI::setFont(const std::filesystem::path& font)
{
    ImFont* fnt = ImGui::GetIO().Fonts->AddFontFromFileTTF(
        font.string().c_str(),
        fontScale * uiSizeFactor
    );

    if (fnt)
        ImGui::GetIO().FontDefault = fnt;
    else
        log::error("Failed to load font: {}", font.string());
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

        GUI::loadedStyle.Colors[ImGuiCol_Border] = {r, g, b, 1};
        GUI::loadedStyle.Colors[ImGuiCol_TitleBg] = {r, g, b, 1};
        GUI::loadedStyle.Colors[ImGuiCol_TitleBgCollapsed] = {r, g, b, 1};
        GUI::loadedStyle.Colors[ImGuiCol_TitleBgActive] = {r, g, b, 1};

        GUI::loadedStyle.Colors[ImGuiCol_Tab] = {r * .85f, g * .85f, b * .85f, 1};
        GUI::loadedStyle.Colors[ImGuiCol_TabActive] = {r * .85f, g * .85f, b * .85f, 1};
        GUI::loadedStyle.Colors[ImGuiCol_TabHovered] = {r * .70f, g * .70f, b * .70f, 1};
        GUI::loadedStyle.Colors[ImGuiCol_TabUnfocused] = {r * .60f, g * .60f, b * .60f, 1};
        GUI::loadedStyle.Colors[ImGuiCol_TabUnfocusedActive] = {r * .60f, g * .60f, b * .60f, 1};
    }
    else
    {
        GUI::loadedStyle.Colors[ImGuiCol_TitleBg] = {r, g, b, 1.f};
        GUI::loadedStyle.Colors[ImGuiCol_TitleBgActive] = {r, g, b, 1.f};
        GUI::loadedStyle.Colors[ImGuiCol_TitleBgCollapsed] = {r, g, b, 1.f};
    }
}

bool GUI::shouldRender()
{
    return isVisible && !shortcutLoop;
}
