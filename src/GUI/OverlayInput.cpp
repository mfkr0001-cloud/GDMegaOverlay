#include "OverlayInput.h"

#if defined(__ANDROID__)

#include "GUI.h"
#include "../Settings.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/CCScene.hpp>
#include <Geode/cocos/touch_dispatcher/CCTouchDispatcher.h>
#include <Geode/cocos/touch_dispatcher/CCTouchDelegateProtocol.h>
#include <Geode/cocos/CCTextureCache.h>
#include <Geode/cocos/CCTexture2D.h>
#include <imgui-cocos.hpp>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <climits>

using namespace geode::prelude;

namespace {
    constexpr int kTouchLayerTag = 0x47444D; // "GDM"
    constexpr float kButtonRadius = 28.f;
    constexpr float kButtonDragThreshold = 8.f;
    constexpr float kScrollThreshold = 10.f;
    constexpr float kScrollPixelsPerStep = 42.f;

    enum class TouchMode {
        None,
        FloatingButton,
        Menu
    };

    ImVec2 s_buttonPosition(-1.f, -1.f);
    bool s_buttonPositionLoaded = false;
    cocos2d::CCTexture2D* s_buttonTexture = nullptr;
    bool s_buttonTextureAttempted = false;

    ImVec2 s_primaryPosition(0.f, 0.f);
    ImVec2 s_secondaryPosition(0.f, 0.f);
    ImVec2 s_lastScrollCenter(0.f, 0.f);
    ImVec2 s_buttonTouchStart(0.f, 0.f);
    ImVec2 s_buttonLastTouch(0.f, 0.f);

    int s_primaryTouch = -1;
    int s_secondaryTouch = -1;
    TouchMode s_touchMode = TouchMode::None;
    bool s_buttonDragged = false;
    bool s_secondaryIsRightClick = false;
    bool s_twoFingerScrolling = false;

    bool pointInsideCircle(const ImVec2& point, const ImVec2& center, float radius) {
        const float dx = point.x - center.x;
        const float dy = point.y - center.y;
        return dx * dx + dy * dy <= radius * radius;
    }

    ImVec2 clampToScreen(ImVec2 point, const ImVec2& screen, float margin) {
        point.x = std::clamp(point.x, margin, std::max(margin, screen.x - margin));
        point.y = std::clamp(point.y, margin, std::max(margin, screen.y - margin));
        return point;
    }

    ImVec2 toImGuiPosition(CCTouch* touch) {
        const CCPoint location = touch->getLocation();
        const CCSize winSize = CCDirector::sharedDirector()->getWinSize();
        const ImVec2 display = ImGui::GetIO().DisplaySize;

        if (winSize.width <= 0.f || winSize.height <= 0.f)
            return ImVec2(0.f, 0.f);

        // Cocos uses a bottom-left origin; ImGui uses a top-left origin.
        return ImVec2(
            location.x * display.x / winSize.width,
            (winSize.height - location.y) * display.y / winSize.height
        );
    }

    void loadButtonPosition(const ImVec2& screen) {
        if (s_buttonPositionLoaded || screen.x <= 0.f || screen.y <= 0.f)
            return;

        const float normalizedX = Mod::get()->getSavedValue<float>("overlay/button-x", 0.065f);
        const float normalizedY = Mod::get()->getSavedValue<float>("overlay/button-y", 0.50f);
        s_buttonPosition = ImVec2(normalizedX * screen.x, normalizedY * screen.y);
        s_buttonPosition = clampToScreen(s_buttonPosition, screen, kButtonRadius + 2.f);
        s_buttonPositionLoaded = true;
    }

    void saveButtonPosition(const ImVec2& screen) {
        if (screen.x <= 0.f || screen.y <= 0.f)
            return;

        Mod::get()->setSavedValue<float>("overlay/button-x", s_buttonPosition.x / screen.x);
        Mod::get()->setSavedValue<float>("overlay/button-y", s_buttonPosition.y / screen.y);
        Mod::get()->saveData();
    }

    void drawEasyFace(ImDrawList* draw, const ImVec2& center, float radius) {
        const ImU32 blue = IM_COL32(42, 143, 255, 248);
        const ImU32 outline = IM_COL32(210, 235, 255, 255);
        const ImU32 white = IM_COL32(255, 255, 255, 255);

        draw->AddCircleFilled(center, radius, blue, 48);
        draw->AddCircle(center, radius, outline, 48, 2.f);

        const float eyeY = center.y - radius * 0.16f;
        const float eyeOffset = radius * 0.31f;
        draw->AddCircleFilled(ImVec2(center.x - eyeOffset, eyeY), radius * 0.105f, white);
        draw->AddCircleFilled(ImVec2(center.x + eyeOffset, eyeY), radius * 0.105f, white);

        draw->PathClear();
        draw->PathArcTo(ImVec2(center.x, center.y + radius * 0.02f), radius * 0.43f,
            0.25f, 2.89f, 24);
        draw->PathStroke(white, ImDrawFlags_None, radius * 0.09f);
    }

    void drawFloatingButton() {
        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 screen = io.DisplaySize;
        if (screen.x <= 0.f || screen.y <= 0.f)
            return;
        loadButtonPosition(screen);
        // Keep the control reachable after rotation or a resolution change.
        s_buttonPosition = clampToScreen(s_buttonPosition, screen, kButtonRadius + 2.f);

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        // A subtle shadow keeps the icon visible over bright gameplay scenes.
        draw->AddCircleFilled(ImVec2(s_buttonPosition.x + 1.f, s_buttonPosition.y + 3.f),
            kButtonRadius + 1.f, IM_COL32(0, 0, 0, 90), 48);

        // The user-provided image must be placed at res/overlay_icon.png.
        // Keep the procedural face as a safe fallback if the image is absent.
        if (!s_buttonTextureAttempted) {
            s_buttonTextureAttempted = true;
            const auto iconPath = (Mod::get()->getResourcesDir() / "overlay_icon.png").string();
            s_buttonTexture = cocos2d::CCTextureCache::get()->addImage(iconPath.c_str(), true);
        }

        if (s_buttonTexture) {
            const ImVec2 size(kButtonRadius * 2.f, kButtonRadius * 2.f);
            const ImVec2 min(s_buttonPosition.x - kButtonRadius, s_buttonPosition.y - kButtonRadius);
            const ImVec2 max(s_buttonPosition.x + kButtonRadius, s_buttonPosition.y + kButtonRadius);
            draw->AddImage((ImTextureID)(intptr_t)s_buttonTexture->getName(), min, max);
        } else {
            drawEasyFace(draw, s_buttonPosition, kButtonRadius);
        }
    }

    void drawCloseButtonAndCursor() {
        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 screen = io.DisplaySize;

        ImGui::SetNextWindowPos(ImVec2(std::max(8.f, screen.x - 60.f), 10.f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(50.f, 50.f), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(24, 28, 36, 225));

        if (ImGui::Begin("##GDMegaOverlayAndroidClose", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing)) {
            if (ImGui::Button("×", ImVec2(50.f, 50.f)))
                GUI::toggle();
        }
        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        ImVec2 pointer = io.MousePos;
        if (pointer.x < 0.f || pointer.y < 0.f || pointer.x > screen.x || pointer.y > screen.y)
            pointer = ImVec2(screen.x * 0.5f, screen.y * 0.5f);

        // Software cursor: independent of Android's system mouse cursor.
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        draw->AddTriangleFilled(pointer,
            ImVec2(pointer.x + 2.f, pointer.y + 22.f),
            ImVec2(pointer.x + 8.f, pointer.y + 15.f),
            IM_COL32(255, 255, 255, 255));
        draw->AddTriangle(pointer,
            ImVec2(pointer.x + 2.f, pointer.y + 22.f),
            ImVec2(pointer.x + 8.f, pointer.y + 15.f),
            IM_COL32(20, 20, 20, 255), 1.2f);
    }

    void resetMenuTouchState() {
        if (ImGui::GetCurrentContext()) {
            ImGuiIO& io = ImGui::GetIO();
            io.AddMouseButtonEvent(0, false);
            io.AddMouseButtonEvent(1, false);
        }
        s_primaryTouch = -1;
        s_secondaryTouch = -1;
        s_touchMode = TouchMode::None;
        s_buttonDragged = false;
        s_secondaryIsRightClick = false;
        s_twoFingerScrolling = false;
        s_primaryPosition = ImVec2(0.f, 0.f);
        s_secondaryPosition = ImVec2(0.f, 0.f);
    }

    class OverlayTouchLayer final : public CCLayer {
    public:
        static OverlayTouchLayer* create() {
            auto* layer = new OverlayTouchLayer();
            if (layer && layer->init()) {
                layer->autorelease();
                return layer;
            }
            CC_SAFE_DELETE(layer);
            return nullptr;
        }

        bool init() override {
            if (!CCLayer::init())
                return false;
            setTouchEnabled(true);
            return true;
        }

        void onExit() override {
            // A scene transition can cancel an in-progress touch. Never leave
            // a stale touch ID or a pressed ImGui button for the next scene.
            resetMenuTouchState();
            s_buttonDragged = false;
            CCLayer::onExit();
        }

        void registerWithTouchDispatcher() override {
            // Highest practical priority. We only claim the Easy button while
            // the menu is hidden; while open, all touches belong to ImGui.
            CCTouchDispatcher::get()->addTargetedDelegate(this, INT_MIN + 1, true);
        }

        bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
            if (!ImGuiCocos::get().isInitialized() || !ImGui::GetCurrentContext())
                return false;

            const int touchId = touch->getID();
            const ImVec2 point = toImGuiPosition(touch);
            const ImVec2 screen = ImGui::GetIO().DisplaySize;

            if (!GUI::toggled) {
                if (s_primaryTouch != -1)
                    return false;

                loadButtonPosition(screen);
                if (!pointInsideCircle(point, s_buttonPosition, kButtonRadius + 12.f))
                    return false;

                s_primaryTouch = touchId;
                s_touchMode = TouchMode::FloatingButton;
                s_buttonTouchStart = point;
                s_buttonLastTouch = point;
                s_buttonDragged = false;
                return true;
            }

            // With the menu open, claim every touch so gameplay never receives
            // menu gestures. The first finger moves/clicks the virtual pointer.
            if (s_touchMode == TouchMode::None || s_touchMode == TouchMode::Menu) {
                if (s_primaryTouch == -1) {
                    s_primaryTouch = touchId;
                    s_touchMode = TouchMode::Menu;
                    s_primaryPosition = point;
                    ImGuiIO& io = ImGui::GetIO();
                    io.AddMousePosEvent(point.x, point.y);
                    io.AddMouseButtonEvent(0, true);
                    return true;
                }

                if (s_secondaryTouch == -1 && touchId != s_primaryTouch) {
                    s_secondaryTouch = touchId;
                    s_secondaryPosition = point;
                    s_lastScrollCenter = ImVec2(
                        (s_primaryPosition.x + s_secondaryPosition.x) * 0.5f,
                        (s_primaryPosition.y + s_secondaryPosition.y) * 0.5f
                    );
                    s_twoFingerScrolling = false;
                    // A two-finger tap is a right-click candidate. Do not
                    // press it yet: if the fingers move, this becomes scroll.
                    s_secondaryIsRightClick = true;
                    ImGui::GetIO().AddMouseButtonEvent(0, false);
                    return true;
                }
            }

            return GUI::toggled;
        }

        void ccTouchMoved(CCTouch* touch, CCEvent*) override {
            if (s_touchMode == TouchMode::FloatingButton && touch->getID() == s_primaryTouch) {
                const ImVec2 point = toImGuiPosition(touch);
                const float dx = point.x - s_buttonTouchStart.x;
                const float dy = point.y - s_buttonTouchStart.y;
                if (!s_buttonDragged && (dx * dx + dy * dy) >= kButtonDragThreshold * kButtonDragThreshold)
                    s_buttonDragged = true;

                if (s_buttonDragged) {
                    s_buttonPosition.x += point.x - s_buttonLastTouch.x;
                    s_buttonPosition.y += point.y - s_buttonLastTouch.y;
                    s_buttonPosition = clampToScreen(s_buttonPosition, ImGui::GetIO().DisplaySize, kButtonRadius + 2.f);
                }
                s_buttonLastTouch = point;
                return;
            }

            if (s_touchMode != TouchMode::Menu || !ImGui::GetCurrentContext())
                return;

            ImGuiIO& io = ImGui::GetIO();
            const int touchId = touch->getID();
            const ImVec2 point = toImGuiPosition(touch);

            if (touchId == s_primaryTouch) {
                s_primaryPosition = point;
                io.AddMousePosEvent(point.x, point.y);
            } else if (touchId == s_secondaryTouch) {
                s_secondaryPosition = point;
            } else {
                return;
            }

            if (s_secondaryTouch != -1) {
                const ImVec2 center(
                    (s_primaryPosition.x + s_secondaryPosition.x) * 0.5f,
                    (s_primaryPosition.y + s_secondaryPosition.y) * 0.5f
                );
                const float deltaY = s_lastScrollCenter.y - center.y;

                if (!s_twoFingerScrolling) {
                    const float totalY = center.y - s_lastScrollCenter.y;
                    if (std::abs(totalY) >= kScrollThreshold) {
                        s_twoFingerScrolling = true;
                        // Movement past the threshold converts the gesture
                        // into scrolling, not a right-click.
                        s_secondaryIsRightClick = false;
                    }
                }

                if (s_twoFingerScrolling && std::abs(deltaY) > 0.1f)
                    io.AddMouseWheelEvent(0.f, deltaY / kScrollPixelsPerStep);

                s_lastScrollCenter = center;
            }
        }

        void ccTouchEnded(CCTouch* touch, CCEvent*) override {
            const int touchId = touch->getID();

            if (s_touchMode == TouchMode::FloatingButton && touchId == s_primaryTouch) {
                const ImVec2 screen = ImGui::GetIO().DisplaySize;
                s_primaryTouch = -1;
                s_touchMode = TouchMode::None;

                if (s_buttonDragged)
                    saveButtonPosition(screen);
                else if (GUI::canToggle)
                    GUI::toggle();

                s_buttonDragged = false;
                return;
            }

            if (s_touchMode != TouchMode::Menu || !ImGui::GetCurrentContext())
                return;

            ImGuiIO& io = ImGui::GetIO();
            if (touchId == s_secondaryTouch) {
                if (s_secondaryIsRightClick) {
                    // Deliver a complete right-click only for a two-finger tap.
                    io.AddMouseButtonEvent(1, true);
                    io.AddMouseButtonEvent(1, false);
                }
                s_secondaryTouch = -1;
                s_secondaryIsRightClick = false;
                s_twoFingerScrolling = false;
                if (s_primaryTouch != -1)
                    io.AddMouseButtonEvent(0, true);
            } else if (touchId == s_primaryTouch) {
                io.AddMouseButtonEvent(0, false);
                s_primaryTouch = -1;
                if (s_secondaryTouch != -1) {
                    s_primaryTouch = s_secondaryTouch;
                    s_primaryPosition = s_secondaryPosition;
                    s_secondaryTouch = -1;
                    // The remaining finger becomes the primary pointer.
                    s_secondaryIsRightClick = false;
                    s_twoFingerScrolling = false;
                    io.AddMousePosEvent(s_primaryPosition.x, s_primaryPosition.y);
                    io.AddMouseButtonEvent(0, true);
                }
            }

            if (s_primaryTouch == -1 && s_secondaryTouch == -1) {
                s_touchMode = TouchMode::None;
                s_secondaryIsRightClick = false;
                s_twoFingerScrolling = false;
            }
        }

        void ccTouchCancelled(CCTouch* touch, CCEvent* event) override {
            if (s_touchMode == TouchMode::FloatingButton) {
                if (touch->getID() == s_primaryTouch) {
                    s_primaryTouch = -1;
                    s_touchMode = TouchMode::None;
                    s_buttonDragged = false;
                }
                return;
            }

            if (s_touchMode == TouchMode::Menu)
                resetMenuTouchState();
        }
    };

    void attachTouchLayer(CCScene* scene) {
        if (!scene || scene->getChildByTag(kTouchLayerTag))
            return;

        if (auto* layer = OverlayTouchLayer::create())
            scene->addChild(layer, INT_MAX, kTouchLayerTag);
    }
}

class $modify(CCScene) {
    void onEnter() {
        CCScene::onEnter();
        attachTouchLayer(this);
    }
};

namespace OverlayInput {
    void install() {
        if (auto* director = CCDirector::sharedDirector())
            attachTouchLayer(director->getRunningScene());
    }

    void draw() {
        if (!ImGui::GetCurrentContext())
            return;

        if (GUI::toggled)
            drawCloseButtonAndCursor();
        else
            drawFloatingButton();
    }
}

#else

namespace OverlayInput {
    void install() {}
    void draw() {}
}

#endif
