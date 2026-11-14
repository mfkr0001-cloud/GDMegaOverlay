#include "StartposSwitcher.h"
#include "../Common.h"
#include "../util.hpp"
#include "../Settings.hpp"
#include "ConstData.h"

#include <algorithm>
#include <string>

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

using namespace StartposSwitcher;
using namespace geode::prelude;

namespace {
    cocos2d::CCMenu* switcherMenu = nullptr;
    cocos2d::CCLabelBMFont* switcherLabel = nullptr;

    void removeSwitcherUI() {
        if (switcherMenu) {
            switcherMenu->removeFromParentAndCleanup(true);
            switcherMenu = nullptr;
            switcherLabel = nullptr;
        }
    }

    void createSwitcherUI(PlayLayer* layer) {
        removeSwitcherUI();

        if (!layer || !layer->m_uiLayer || startposObjects.empty())
            return;

        auto director = cocos2d::CCDirector::sharedDirector();
        const auto winSize = director->getWinSize();
        const float bottom = director->getScreenBottom();

        switcherMenu = cocos2d::CCMenu::create();
        if (!switcherMenu)
            return;

        switcherMenu->setID("megaOverlay.startposSwitcher"_spr);
        switcherMenu->setPosition(0.f, 0.f);
        switcherMenu->setZOrder(999);
        switcherMenu->setCascadeOpacityEnabled(true);

        switcherLabel = cocos2d::CCLabelBMFont::create("", "bigFont.fnt");
        if (switcherLabel) {
            switcherLabel->setScale(0.44f);
            switcherLabel->setPosition(winSize.width / 2.f, bottom + 56.f);
            switcherLabel->setID("megaOverlay.startposSwitcher.label"_spr);
            switcherMenu->addChild(switcherLabel);
        }

        auto leftSprite = cocos2d::CCSprite::createWithSpriteFrameName("GJ_arrow_02_001.png");
        if (leftSprite) {
            leftSprite->setScale(0.42f);
            auto leftButton = geode::cocos::CCMenuItemExt::createSpriteExtra(
                leftSprite,
                [](CCMenuItemSpriteExtra*) { change(false); }
            );
            leftButton->setPosition(winSize.width / 2.f - 70.f, bottom + 20.f);
            leftButton->setID("megaOverlay.startposSwitcher.left"_spr);
            switcherMenu->addChild(leftButton);
        }

        auto rightSprite = cocos2d::CCSprite::createWithSpriteFrameName("GJ_arrow_02_001.png");
        if (rightSprite) {
            rightSprite->setScale(0.42f);
            rightSprite->setFlipX(true);
            auto rightButton = geode::cocos::CCMenuItemExt::createSpriteExtra(
                rightSprite,
                [](CCMenuItemSpriteExtra*) { change(true); }
            );
            rightButton->setPosition(winSize.width / 2.f + 70.f, bottom + 20.f);
            rightButton->setID("megaOverlay.startposSwitcher.right"_spr);
            switcherMenu->addChild(rightButton);
        }

        layer->m_uiLayer->addChild(switcherMenu);
    }
}

class $modify(CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool arr, double time) {
        if (down && !arr) {
            const int leftKey = Settings::get<int>(
                "level/startpos_switcher/left", ImGuiKey_LeftArrow
            );
            const int rightKey = Settings::get<int>(
                "level/startpos_switcher/right", ImGuiKey_RightArrow
            );

            const int convertedKey = ConvertKeyEnum(key);
            if (convertedKey == leftKey)
                change(false);
            else if (convertedKey == rightKey)
                change(true);
        }

        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, arr, time);
    }
};

class $modify(PlayLayer) {
    #if !defined(__ANDROID__)
bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        removeSwitcherUI();
        startposObjects.clear();
        index = -1;

        return PlayLayer::init(level, useReplay, dontCreateObjects);
    }
#endif

    void addObject(GameObject* object) {
        PlayLayer::addObject(object);

        if (object && object->m_objectID == 31)
            startposObjects.push_back(static_cast<StartPosObject*>(object));
    }

    void createObjectsFromSetupFinished() {
        PlayLayer::createObjectsFromSetupFinished();

        std::sort(
            startposObjects.begin(),
            startposObjects.end(),
            [](StartPosObject* a, StartPosObject* b) {
                return a->getPositionX() < b->getPositionX();
            }
        );

        index = -1;
        if (m_startPosObject) {
            for (int i = 0; i < static_cast<int>(startposObjects.size()); ++i) {
                if (startposObjects[i] == m_startPosObject) {
                    index = i;
                    break;
                }
            }
        }

        if (Settings::get<bool>("level/startpos_switcher", false))
            createSwitcherUI(this);

        showLabel();
    }

    void onQuit() {
        removeSwitcherUI();
        startposObjects.clear();
        index = -1;
        PlayLayer::onQuit();
    }
};

void StartposSwitcher::change(bool right) {
    auto* layer = PlayLayer::get();
    if (!layer ||
        !Settings::get<bool>("level/startpos_switcher", false) ||
        startposObjects.empty() ||
        layer->m_levelEndAnimationStarted)
        return;

    if (right)
        ++index;
    else
        --index;

    if (index < -1)
        index = static_cast<int>(startposObjects.size()) - 1;
    else if (index >= static_cast<int>(startposObjects.size()))
        index = -1;

    loadStartPos();
    showLabel();
}

void StartposSwitcher::loadStartPos() {
    auto* layer = PlayLayer::get();
    if (!layer)
        return;

    StartPosObject* startPos = index == -1 ? nullptr : startposObjects[index];
    if (!startPos && index != -1)
        return;

    const bool wasPracticeMode = layer->m_isPracticeMode;
    if (wasPracticeMode)
        layer->togglePracticeMode(false);

    layer->m_isTestMode = (startPos != nullptr);
    layer->m_currentCheckpoint = nullptr;
    layer->setStartPosObject(startPos);
    layer->resetLevel();
    layer->startMusic();

    if (wasPracticeMode)
        layer->togglePracticeMode(true);
}

void StartposSwitcher::showLabel() {
    if (!switcherLabel || startposObjects.empty())
        return;

    const std::string text = fmt::format(
        "{}/{}",
        index + 1,
        startposObjects.size()
    );
    switcherLabel->setString(text.c_str());

    if (switcherMenu) {
        switcherMenu->stopAllActions();
        switcherMenu->setOpacity(210);
        switcherMenu->runAction(cocos2d::CCSequence::create(
            cocos2d::CCDelayTime::create(2.f),
            cocos2d::CCFadeTo::create(0.35f, 125),
            nullptr
        ));
    }
}
