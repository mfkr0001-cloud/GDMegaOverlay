#include "OverlayInput.h"

#if defined(__ANDROID__)

#include "GUI.h"

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/cocos/touch_dispatcher/CCTouchDispatcher.h>
#include <Geode/cocos/textures/CCTextureCache.h>

#include <imgui-cocos.hpp>
#include <imgui_internal.h>

#include <algorithm>
#include <climits>
#include <cmath>

using namespace geode::prelude;
using namespace cocos2d;

namespace {
    constexpr int kFloatingButtonTag = 0x47444D31;
    constexpr int kImGuiTouchLayerTag = 0x47444D32;
    constexpr float kDragThreshold = 8.f;

    ImVec2 cocosToImGui(const CCPoint& point) {
        const CCSize win = CCDirector::sharedDirector()->getWinSize();
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        if (win.width <= 0.f || win.height <= 0.f)
            return ImVec2(0.f, 0.f);

        return ImVec2(
            point.x * display.x / win.width,
            (win.height - point.y) * display.y / win.height
        );
    }

    void drawCloseButton() {
        const ImVec2 screen = ImGui::GetIO().DisplaySize;

        ImGui::SetNextWindowPos(
            ImVec2(std::max(8.f, screen.x - 48.f), 8.f),
            ImGuiCond_Always
        );
        ImGui::SetNextWindowSize(ImVec2(40.f, 40.f), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(24, 28, 36, 225));

        if (ImGui::Begin(
            "##GDMegaOverlayAndroidClose",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing
        )) {
            if (ImGui::Button("×", ImVec2(40.f, 40.f)))
                GUI::toggle();
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    class FloatingButton final : public CCLayer {
        CCSprite* m_sprite = nullptr;
        CCPoint m_target = CCPoint(0.f, 0.f);
        CCPoint m_lastTouch = CCPoint(0.f, 0.f);
        CCPoint m_touchStart = CCPoint(0.f, 0.f);
        float m_scale = 0.30f;
        bool m_dragged = false;
        bool m_registered = false;

        float radius() const {
            return m_sprite ? m_sprite->getScaledContentSize().width * 0.5f : 28.f;
        }

        void clampTarget() {
            const CCSize size = CCDirector::sharedDirector()->getWinSize();
            const float r = radius() + 2.f;
            m_target.x = std::clamp(m_target.x, r, std::max(r, size.width - r));
            m_target.y = std::clamp(m_target.y, r, std::max(r, size.height - r));
        }

        void savePosition() {
            const CCSize size = CCDirector::sharedDirector()->getWinSize();
            if (size.width <= 0.f || size.height <= 0.f)
                return;

            Mod::get()->setSavedValue<float>("overlay/button-x", m_target.x / size.width);
            Mod::get()->setSavedValue<float>("overlay/button-y", m_target.y / size.height);
            Mod::get()->saveData();
        }

        void addTouchDelegate() {
            if (m_registered || !isVisible())
                return;
            CCTouchDispatcher::get()->addTargetedDelegate(this, -1000, true);
            m_registered = true;
        }

        void removeTouchDelegate() {
            if (!m_registered)
                return;
            CCTouchDispatcher::get()->removeDelegate(this);
            m_registered = false;
        }

    public:
        static FloatingButton* create() {
            auto* ret = new FloatingButton();
            if (ret && ret->init()) {
                ret->autorelease();
                return ret;
            }
            CC_SAFE_DELETE(ret);
            return nullptr;
        }

        bool init() override {
            if (!CCLayer::init())
                return false;

            const CCSize size = CCDirector::sharedDirector()->getWinSize();
            setContentSize(size);
            setAnchorPoint(CCPoint(0.f, 0.f));
            setPosition(CCPoint(0.f, 0.f));

            const auto resourcePath = (Mod::get()->getResourcesDir() / "overlay_icon.png").string();
            auto* texture = CCTextureCache::get()->addImage(resourcePath.c_str(), true);
            if (!texture) {
                log::error("GDMegaOverlay: cannot load res/overlay_icon.png");
                return false;
            }

            m_sprite = CCSprite::createWithTexture(texture);
            if (!m_sprite)
                return false;

            m_sprite->setScale(m_scale);
            addChild(m_sprite);

            const float savedX = Mod::get()->getSavedValue<float>("overlay/button-x", 0.065f);
            const float savedY = Mod::get()->getSavedValue<float>("overlay/button-y", 0.50f);
            m_target = CCPoint(savedX * size.width, savedY * size.height);
            clampTarget();
            m_sprite->setPosition(m_target);

            setZOrder(INT_MAX - 1);
            scheduleUpdate();
            return true;
        }

        void onEnter() override {
            CCLayer::onEnter();
            addTouchDelegate();
        }

        void onExit() override {
            removeTouchDelegate();
            CCLayer::onExit();
        }

        void registerWithTouchDispatcher() override {
            addTouchDelegate();
        }

        void setVisible(bool visible) override {
            if (isVisible() == visible)
                return;

            CCLayer::setVisible(visible);
            if (visible)
                addTouchDelegate();
            else
                removeTouchDelegate();
        }

        void update(float dt) override {
            if (!m_sprite)
                return;

            // Hide the floating control while the ImGui overlay is open.
            setVisible(!GUI::toggled);

            const CCSize size = CCDirector::sharedDirector()->getWinSize();
            if (getContentSize().width != size.width || getContentSize().height != size.height)
                setContentSize(size);

            clampTarget();
            const CCPoint current = m_sprite->getPosition();
            if (ccpDistance(current, m_target) > 0.5f)
                m_sprite->setPosition(ccpLerp(current, m_target, std::min(1.f, 12.f * dt)));
        }

        bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
            if (!isVisible() || GUI::toggled || !m_sprite)
                return false;

            const CCPoint point = convertToNodeSpace(touch->getLocation());
            if (ccpDistance(point, m_sprite->getPosition()) > radius() + 10.f)
                return false;

            m_touchStart = point;
            m_lastTouch = point;
            m_dragged = false;
            m_sprite->stopAllActions();
            m_sprite->runAction(CCScaleTo::create(0.08f, m_scale * 0.9f));
            return true;
        }

        void ccTouchMoved(CCTouch* touch, CCEvent*) override {
            const CCPoint point = convertToNodeSpace(touch->getLocation());
            const float dx = point.x - m_touchStart.x;
            const float dy = point.y - m_touchStart.y;

            if (!m_dragged && dx * dx + dy * dy >= kDragThreshold * kDragThreshold)
                m_dragged = true;

            if (m_dragged) {
                m_target.x += point.x - m_lastTouch.x;
                m_target.y += point.y - m_lastTouch.y;
                clampTarget();
            }

            m_lastTouch = point;
        }

        void ccTouchEnded(CCTouch*, CCEvent*) override {
            if (!m_sprite)
                return;

            m_sprite->stopAllActions();
            m_sprite->runAction(CCEaseBackOut::create(CCScaleTo::create(0.22f, m_scale)));

            if (m_dragged)
                savePosition();
            else
                GUI::toggle();

            m_dragged = false;
        }

        void ccTouchCancelled(CCTouch* touch, CCEvent* event) override {
            ccTouchEnded(touch, event);
        }
    };

    class ImGuiTouchLayer final : public CCLayer {
        bool m_registered = false;

        void addTouchDelegate() {
            if (m_registered)
                return;
            CCTouchDispatcher::get()->addTargetedDelegate(this, INT_MIN, true);
            m_registered = true;
        }

        void removeTouchDelegate() {
            if (!m_registered)
                return;
            CCTouchDispatcher::get()->removeDelegate(this);
            m_registered = false;
        }

    public:
        static ImGuiTouchLayer* create() {
            auto* ret = new ImGuiTouchLayer();
            if (ret && ret->init()) {
                ret->autorelease();
                return ret;
            }
            CC_SAFE_DELETE(ret);
            return nullptr;
        }

        bool init() override {
            if (!CCLayer::init())
                return false;
            return true;
        }

        void onEnter() override {
            CCLayer::onEnter();
            addTouchDelegate();
        }

        void onExit() override {
            removeTouchDelegate();
            CCLayer::onExit();
        }

        void registerWithTouchDispatcher() override {
            addTouchDelegate();
        }

        bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
            if (!GUI::toggled || !ImGuiCocos::get().isInitialized() || !ImGui::GetCurrentContext())
                return false;

            const ImVec2 point = cocosToImGui(touch->getLocation());
            ImGuiIO& io = ImGui::GetIO();
            io.AddMousePosEvent(point.x, point.y);
            io.AddMouseButtonEvent(0, true);
            return true;
        }

        void ccTouchMoved(CCTouch* touch, CCEvent*) override {
            if (!GUI::toggled || !ImGui::GetCurrentContext())
                return;

            const ImVec2 point = cocosToImGui(touch->getLocation());
            ImGui::GetIO().AddMousePosEvent(point.x, point.y);
        }

        void ccTouchEnded(CCTouch* touch, CCEvent*) override {
            if (!ImGui::GetCurrentContext())
                return;

            const ImVec2 point = cocosToImGui(touch->getLocation());
            ImGuiIO& io = ImGui::GetIO();
            io.AddMousePosEvent(point.x, point.y);
            io.AddMouseButtonEvent(0, false);
        }

        void ccTouchCancelled(CCTouch*, CCEvent*) override {
            if (ImGui::GetCurrentContext())
                ImGui::GetIO().AddMouseButtonEvent(0, false);
        }
    };

    void attachOverlayToNode(CCNode* parent) {
        if (!parent)
            return;

        if (!parent->getChildByTag(kFloatingButtonTag)) {
            if (auto* button = FloatingButton::create())
                parent->addChild(button, INT_MAX - 1, kFloatingButtonTag);
        }

        if (!parent->getChildByTag(kImGuiTouchLayerTag)) {
            if (auto* touchLayer = ImGuiTouchLayer::create())
                parent->addChild(touchLayer, INT_MAX, kImGuiTouchLayerTag);
        }
    }

    void syncFloatingButtonVisibility() {
        auto* director = CCDirector::sharedDirector();
        if (!director)
            return;

        auto* scene = director->getRunningScene();
        if (!scene)
            return;

        // The button is attached to MenuLayer or PauseLayer, not to loading/game scenes.
        auto* node = scene->getChildByTag(kFloatingButtonTag);
        if (!node) {
            if (auto* menu = typeinfo_cast<MenuLayer*>(scene))
                node = menu->getChildByTag(kFloatingButtonTag);
        }

        if (node)
            static_cast<FloatingButton*>(node)->setVisible(!GUI::toggled);
    }
}

class $modify(GDMegaOverlayMenuHook, MenuLayer) {
    bool init() {
        if (!MenuLayer::init())
            return false;

        attachOverlayToNode(this);
        return true;
    }
};

class $modify(GDMegaOverlayPauseHook, PauseLayer) {
    void onEnter() {
        PauseLayer::onEnter();
        attachOverlayToNode(this);
    }
};

namespace OverlayInput {
    void install() {
        // Do not create the floating button in the loading scene.
        // MenuLayer::init and PauseLayer::onEnter attach it to the correct screens.
    }

    void setPaused(bool) {}

    void draw() {
        if (!ImGui::GetCurrentContext())
            return;

        if (GUI::toggled)
            drawCloseButton();
    }
}

#else

namespace OverlayInput {
    void install() {}
    void draw() {}
    void setPaused(bool) {}
}

#endif
