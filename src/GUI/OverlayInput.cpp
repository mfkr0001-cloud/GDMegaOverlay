
#include "OverlayInput.h"

#if defined(__ANDROID__)

#include "GUI.h"
#include "../Settings.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/CCScene.hpp>
#include <Geode/cocos/touch_dispatcher/CCTouchDispatcher.h>
#include <Geode/cocos/touch_dispatcher/CCTouchDelegateProtocol.h>
#include <Geode/cocos/textures/CCTextureCache.h>
#include <Geode/cocos/textures/CCTexture2D.h>
#include <imgui-cocos.hpp>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <climits>
#include <cstdint>

using namespace geode::prelude;

namespace {
    constexpr int kTouchLayerTag = 0x47444D;
    constexpr float kButtonRadius = 28.f;
    constexpr float kDragThreshold = 8.f;

    enum class TouchMode {
        None,
        FloatingButton,
        Menu
    };

    ImVec2 s_buttonPosition(-1.f, -1.f);
    bool s_buttonPositionLoaded = false;

    int s_primaryTouch = -1;
    TouchMode s_touchMode = TouchMode::None;

    bool s_buttonDragged = false;
    ImVec2 s_buttonTouchStart(0.f, 0.f);
    ImVec2 s_buttonLastTouch(0.f, 0.f);

    cocos2d::CCTexture2D* s_buttonTexture = nullptr;
    bool s_textureAttempted = false;

    bool pointInsideCircle(
        const ImVec2& point,
        const ImVec2& center,
        float radius
    ) {
        float dx = point.x - center.x;
        float dy = point.y - center.y;
        return dx * dx + dy * dy <= radius * radius;
    }

    ImVec2 clampPosition(ImVec2 point, const ImVec2& screen) {
        point.x = std::clamp(
            point.x,
            kButtonRadius + 2.f,
            std::max(kButtonRadius + 2.f, screen.x - kButtonRadius - 2.f)
        );

        point.y = std::clamp(
            point.y,
            kButtonRadius + 2.f,
            std::max(kButtonRadius + 2.f, screen.y - kButtonRadius - 2.f)
        );

        return point;
    }

    ImVec2 getTouchPosition(CCTouch* touch) {
        auto location = touch->getLocation();
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto display = ImGui::GetIO().DisplaySize;

        if (winSize.width <= 0.f || winSize.height <= 0.f)
            return ImVec2(0.f, 0.f);

        return ImVec2(
            location.x * display.x / winSize.width,
            (winSize.height - location.y) * display.y / winSize.height
        );
    }

    void loadButtonPosition(const ImVec2& screen) {
        if (s_buttonPositionLoaded || screen.x <= 0.f || screen.y <= 0.f)
            return;

        float x = Mod::get()->getSavedValue<float>(
            "overlay/button-x", 0.065f
        );

        float y = Mod::get()->getSavedValue<float>(
            "overlay/button-y", 0.50f
        );

        s_buttonPosition = ImVec2(x * screen.x, y * screen.y);
        s_buttonPosition = clampPosition(s_buttonPosition, screen);

        s_buttonPositionLoaded = true;
    }

    void saveButtonPosition(const ImVec2& screen) {
        if (screen.x <= 0.f || screen.y <= 0.f)
            return;

        Mod::get()->setSavedValue<float>(
            "overlay/button-x",
            s_buttonPosition.x / screen.x
        );

        Mod::get()->setSavedValue<float>(
            "overlay/button-y",
            s_buttonPosition.y / screen.y
        );

        Mod::get()->saveData();
    }

    void drawFace(ImDrawList* draw, const ImVec2& center) {
        const ImU32 blue = IM_COL32(42, 143, 255, 248);
        const ImU32 outline = IM_COL32(210, 235, 255, 255);
        const ImU32 white = IM_COL32(255, 255, 255, 255);

        draw->AddCircleFilled(center, kButtonRadius, blue, 48);
        draw->AddCircle(center, kButtonRadius, outline, 48, 2.f);

        float eyeY = center.y - kButtonRadius * 0.16f;
        float eyeOffset = kButtonRadius * 0.31f;

        draw->AddCircleFilled(
            ImVec2(center.x - eyeOffset, eyeY),
            kButtonRadius * 0.105f,
            white
        );

        draw->AddCircleFilled(
            ImVec2(center.x + eyeOffset, eyeY),
            kButtonRadius * 0.105f,
            white
        );

        draw->PathClear();
        draw->PathArcTo(
            ImVec2(center.x, center.y + kButtonRadius * 0.02f),
            kButtonRadius * 0.43f,
            0.25f,
            2.89f,
            24
        );
        draw->PathStroke(
            white,
            ImDrawFlags_None,
            kButtonRadius * 0.09f
        );
    }

    void drawFloatingButton() {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 screen = io.DisplaySize;

        if (screen.x <= 0.f || screen.y <= 0.f)
            return;

        loadButtonPosition(screen);
        s_buttonPosition = clampPosition(s_buttonPosition, screen);

        ImDrawList* draw = ImGui::GetForegroundDrawList();

        draw->AddCircleFilled(
            ImVec2(s_buttonPosition.x + 1.f, s_buttonPosition.y + 3.f),
            kButtonRadius + 1.f,
            IM_COL32(0, 0, 0, 90),
            48
        );

        if (!s_textureAttempted) {
            s_textureAttempted = true;

            auto path = (
                Mod::get()->getResourcesDir() / "overlay_icon.png"
            ).string();

            s_buttonTexture =
                cocos2d::CCTextureCache::get()->addImage(
                    path.c_str(),
                    true
                );
        }

        if (s_buttonTexture) {
            ImVec2 min(
                s_buttonPosition.x - kButtonRadius,
                s_buttonPosition.y - kButtonRadius
            );

            ImVec2 max(
                s_buttonPosition.x + kButtonRadius,
                s_buttonPosition.y + kButtonRadius
            );

            draw->AddImage(
                (ImTextureID)(intptr_t)s_buttonTexture->getName(),
                min,
                max
            );
        } else {
            drawFace(draw, s_buttonPosition);
        }
    }

    void drawCloseButton() {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 screen = io.DisplaySize;

        ImGui::SetNextWindowPos(
            ImVec2(std::max(8.f, screen.x - 60.f), 10.f),
            ImGuiCond_Always
        );

        ImGui::SetNextWindowSize(
            ImVec2(50.f, 50.f),
            ImGuiCond_Always
        );

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding,
            ImVec2(0.f, 0.f)
        );

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowBorderSize,
            0.f
        );

        ImGui::PushStyleColor(
            ImGuiCol_WindowBg,
            IM_COL32(24, 28, 36, 225)
        );

        if (ImGui::Begin(
            "##GDMegaOverlayAndroidClose",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing
        )) {
            if (ImGui::Button("×", ImVec2(50.f, 50.f))) {
                GUI::toggle();
            }
        }

        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    void resetTouch() {
        if (ImGui::GetCurrentContext()) {
            ImGuiIO& io = ImGui::GetIO();
            io.AddMouseButtonEvent(0, false);
            io.AddMouseButtonEvent(1, false);
        }

        s_primaryTouch = -1;
        s_touchMode = TouchMode::None;
        s_buttonDragged = false;
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

        void registerWithTouchDispatcher() override {
            CCTouchDispatcher::get()->addTargetedDelegate(
                this,
                INT_MIN,
                true
            );
        }

        void onExit() override {
            resetTouch();
            CCLayer::onExit();
        }

        bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
            if (!ImGuiCocos::get().isInitialized())
                return false;

            if (!ImGui::GetCurrentContext())
                return false;

            if (s_primaryTouch != -1)
                return false;

            ImVec2 point = getTouchPosition(touch);
            ImVec2 screen = ImGui::GetIO().DisplaySize;

            if (!GUI::toggled) {
                loadButtonPosition(screen);

                if (!pointInsideCircle(
                    point,
                    s_buttonPosition,
                    kButtonRadius + 14.f
                )) {
                    return false;
                }

                s_primaryTouch = touch->getID();
                s_touchMode = TouchMode::FloatingButton;

                s_buttonTouchStart = point;
                s_buttonLastTouch = point;
                s_buttonDragged = false;

                return true;
            }

            s_primaryTouch = touch->getID();
            s_touchMode = TouchMode::Menu;

            ImGuiIO& io = ImGui::GetIO();
            io.AddMousePosEvent(point.x, point.y);
            io.AddMouseButtonEvent(0, true);

            return true;
        }

        void ccTouchMoved(CCTouch* touch, CCEvent*) override {
            if (touch->getID() != s_primaryTouch)
                return;

            ImVec2 point = getTouchPosition(touch);

            if (s_touchMode == TouchMode::FloatingButton) {
                float dx = point.x - s_buttonTouchStart.x;
                float dy = point.y - s_buttonTouchStart.y;

                if (!s_buttonDragged &&
                    dx * dx + dy * dy >=
                    kDragThreshold * kDragThreshold) {
                    s_buttonDragged = true;
                }

                if (s_buttonDragged) {
                    s_buttonPosition.x += point.x - s_buttonLastTouch.x;
                    s_buttonPosition.y += point.y - s_buttonLastTouch.y;

                    s_buttonPosition = clampPosition(
                        s_buttonPosition,
                        ImGui::GetIO().DisplaySize
                    );
                }

                s_buttonLastTouch = point;
                return;
            }

            if (s_touchMode == TouchMode::Menu &&
                ImGui::GetCurrentContext()) {
                ImGui::GetIO().AddMousePosEvent(point.x, point.y);
            }
        }

        void ccTouchEnded(CCTouch* touch, CCEvent*) override {
            if (touch->getID() != s_primaryTouch)
                return;

            if (s_touchMode == TouchMode::FloatingButton) {
                ImVec2 screen = ImGui::GetIO().DisplaySize;

                if (s_buttonDragged) {
                    saveButtonPosition(screen);
                } else {
                    // A normal tap always opens the menu.
                    // Do not block it with GUI::canToggle.
                    GUI::toggle();
                }

                s_primaryTouch = -1;
                s_touchMode = TouchMode::None;
                s_buttonDragged = false;
                return;
            }

            if (s_touchMode == TouchMode::Menu &&
                ImGui::GetCurrentContext()) {
                ImGui::GetIO().AddMouseButtonEvent(0, false);
            }

            s_primaryTouch = -1;
            s_touchMode = TouchMode::None;
        }

        void ccTouchCancelled(CCTouch* touch, CCEvent*) override {
            if (touch->getID() == s_primaryTouch)
                resetTouch();
        }
    };

    void attachTouchLayer(CCScene* scene) {
        if (!scene)
            return;

        if (scene->getChildByTag(kTouchLayerTag))
            return;

        if (auto* layer = OverlayTouchLayer::create()) {
            scene->addChild(layer, INT_MAX, kTouchLayerTag);
        }
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
        auto* director = CCDirector::sharedDirector();

        if (director) {
            attachTouchLayer(director->getRunningScene());
        }
    }

    void draw() {
        if (!ImGui::GetCurrentContext())
            return;

        if (GUI::toggled)
            drawCloseButton();
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
