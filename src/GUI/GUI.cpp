
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

#if defined(__ANDROID__)
namespace GUI
{
    // Implemented in Window.cpp
    ImVec2 androidScreenSize();
    bool fitWindowToScreen(Window& window);
}
#endif

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
    if (!windowPositions.contains(name))
    {
        windowPositions[name]["x"] = .0f;
        windowPositions[name]["y"] = .0f;
    }

#if defined(__ANDROID__)
    // Saved Android window coordinates are normalized against ImGui's logical
    // display size (the same coordinate space used by Window::visibleInScreen).
    // If ImGui has no display size yet, the cocos frame size is used instead.
    const ImVec2 androidScreen = androidScreenSize();
    auto winSize = cocos2d::CCSize(androidScreen.x, androidScreen.y);
#else
    auto winSize = CCDirector::sharedDirector()->getOpenGLView()->getViewPortRect().size;
#endif

    if (windowPositions[name]["x"] > 1 || windowPositions[name]["y"] > 1)
        winSize = cocos2d::CCSize(1.f, 1.f);

    return {
        windowPositions[name]["x"].get<float>() * winSize.width,
        windowPositions[name]["y"].get<float>() * winSize.height
    };
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

#if defined(__ANDROID__)
    const ImVec2 androidScreen = androidScreenSize();
    auto winSize = cocos2d::CCSize(androidScreen.x, androidScreen.y);
#else
    auto winSize = CCDirector::sharedDirector()->getOpenGLView()->getViewPortRect().size;
#endif

    if (windowPositions[name]["w"] > 1 || windowPositions[name]["h"] > 1)
        winSize = cocos2d::CCSize(1.f, 1.f);

    return {
        windowPositions[name]["w"].get<float>() * winSize.width,
        windowPositions[name]["h"].get<float>() * winSize.height
    };
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
    uiSizeFactor *= Settings::get<float>("menu/ui_scale", 1.f);
#if defined(__ANDROID__)
    // Apply the Android scale before fonts and default window sizes are created.
    // This keeps text, widgets, tabs and window geometry in the same scale.
    uiSizeFactor *= 0.42f;
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

#if defined(__ANDROID__)
    // Apply the Android tiled layout (4 columns x 3 rows, every window inside
    // the screen) once per layout revision.
    // Existing user layouts are preserved on later launches.
    constexpr int kAndroidLayoutRevision = 7;
    const int appliedLayoutRevision =
        Mod::get()->getSavedValue<int>("android-layout-revision", 0);
    if (appliedLayoutRevision < kAndroidLayoutRevision) {
        GUI::resetDefault();
        GUI::save();
        Mod::get()->setSavedValue<int>(
            "android-layout-revision",
            kAndroidLayoutRevision
        );
        Mod::get()->saveData();
    }
#endif
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
    uiSizeFactor *= 0.42f;
#endif

    if (!isVisible && !shortcutLoop)
        return;

    ImGui::GetStyle() = loadedStyle;
    ImGui::GetStyle().ScaleAllSizes(uiSizeFactor);
    ImGui::GetStyle().FramePadding *= 0.90f;
    ImGui::GetStyle().ItemSpacing *= 0.90f;
    ImGui::GetStyle().ItemInnerSpacing *= 0.90f;

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

#if defined(__ANDROID__)
    // Make sure every window target position is on screen before animating.
    for (Window& w : windows)
        fitWindowToScreen(w);
#endif

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

#if defined(__ANDROID__)
    fitWindowToScreen(window);
#endif

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
#if defined(__ANDROID__)
    ImVec2 saveScreen = androidScreenSize();
#else
    ImVec2 saveScreen = ImGui::GetIO().DisplaySize;
#endif

    // Never divide by a zero display size: it would write inf/NaN into the json.
    if (saveScreen.x > 1.f && saveScreen.y > 1.f)
    {
        for (Window& w : windows)
        {
#if defined(__ANDROID__)
            fitWindowToScreen(w);
#endif
            windowPositions[w.name]["x"] = (float)w.position.x / saveScreen.x;
            windowPositions[w.name]["y"] = (float)w.position.y / saveScreen.y;
            windowPositions[w.name]["w"] = (float)w.size.x / saveScreen.x;
            windowPositions[w.name]["h"] = (float)w.size.y / saveScreen.y;
        }
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

#if defined(__ANDROID__)
// Tiled Android layout in normalized (0..1) screen coordinates:
// 4 columns x 3 rows, every window fully inside the screen, readable width.
static void applyAndroidDefaultLayout()
{
    constexpr float kMarginX = 0.012f;
    constexpr float kMarginY = 0.02f;
    constexpr float kGapY = 0.02f;
    constexpr int kCols = 4;
    constexpr int kRows = 3;
    constexpr float kCellW = (1.f - kMarginX * (kCols + 1)) / kCols; // ~0.238
    constexpr float kCellH = (1.f - kMarginY * 2.f - kGapY * (kRows - 1)) / kRows; // ~0.307

    static const char* const order[kRows][kCols] = {
        {"General", "Global", "Level", "Player"},
        {"Bypass", "Creator", "Macro Bot", "Recorder"},
        {"Labels", "Shortcuts", "Menu Settings", "Recording"},
    };

    for (int r = 0; r < kRows; ++r)
    {
        for (int c = 0; c < kCols; ++c)
        {
            json& entry = GUI::windowPositions[order[r][c]];
            entry["x"] = kMarginX + c * (kCellW + kMarginX);
            entry["y"] = kMarginY + r * (kCellH + kGapY);
            entry["w"] = kCellW;
            entry["h"] = kCellH;
        }
    }

    // Style Editor is opened on demand: big, centered window.
    json& style = GUI::windowPositions["Style Editor"];
    style["x"] = 0.20f;
    style["y"] = 0.08f;
    style["w"] = 0.60f;
    style["h"] = 0.84f;
}
#endif

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

    if (!windowPositions.is_object())
        windowPositions = json::object();

#if defined(__ANDROID__)
    // Desktop defaults are made for a 1920px wide window; on Android they give
    // narrow columns that run off the screen, so use the tiled layout instead.
    applyAndroidDefaultLayout();
#endif

    for (Window& window : windows)
    {
        window.position = getJsonPosition(window.name);
        window.size = getJsonSize(window.name, window.size);
#if defined(__ANDROID__)
        window.renderPosition = window.position;
        fitWindowToScreen(window);
#endif
        window.renderPosition = window.position;
    }
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
    // Reduce only the text size; keep window dimensions and widget spacing unchanged.
    ImFont* fnt = ImGui::GetIO().Fonts->AddFontFromFileTTF(
        font.string().c_str(),
        fontScale * uiSizeFactor * 0.65f
    );

    if (fnt)
        ImGui::GetIO().FontDefault = fnt;
    else
        log::error("Failed to load font: {}", font.string());
}

void GUI::setStyle()
{
    float r = Settings::get<float>("menu/window/color/r", 0.90f);
    float g = Settings::get<float>("menu/window/color/g", 0.18f);
    float b = Settings::get<float>("menu/window/color/b", 0.63f);

    // Match the dark plum panels in the reference overlay while leaving
    // the user's opacity, blur and title color controls available.
    GUI::loadedStyle.Colors[ImGuiCol_WindowBg] = {0.075f, 0.035f, 0.080f, 1.0f};

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
