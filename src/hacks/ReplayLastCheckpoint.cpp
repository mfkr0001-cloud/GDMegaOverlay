#include "ReplayLastCheckpoint.h"
#include "../Common.h"

#include "../util.hpp"
#include "../Settings.hpp"

#include <Geode/Geode.hpp>
#include <cocos2d.h>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/EndLevelLayer.hpp>

using namespace geode::prelude;
using namespace ReplayLastCheckpoint;

class $modify(PlayLayer)
{
    void levelComplete()
    {
        levelCompleted = true;
        PlayLayer::levelComplete();
    }

    void resetLevel()
    {
        levelCompleted = false;
        PlayLayer::resetLevel();
    }
};

class $modify(ButtonsClass, EndLevelLayer)
{
    void onReset(CCObject* sender)
    {
        levelCompleted = false;

        // Close the end-level layer.
        this->getParent()->getParent()->removeFromParent();

        PlayLayer::get()->resetLevel();
    }

    void customSetup()
    {
        EndLevelLayer::customSetup();

        auto playLayer = PlayLayer::get();

        if (!Settings::get<bool>("level/replay_checkpoint") ||
            !playLayer ||
            !playLayer->m_isPracticeMode)
            return;

        auto layer = typeinfo_cast<CCLayer*>(
            this->getChildren()->objectAtIndex(0)
        );

        if (!layer)
            return;

        CCMenu* buttonsMenu = nullptr;

        // Geode v5 replacement for CCARRAY_FOREACH.
        for (auto object : CCArrayExt(layer->getChildren()))
        {
            if (auto menu = typeinfo_cast<CCMenu*>(object);
                menu && menu->getChildrenCount() > 1)
            {
                buttonsMenu = menu;
                break;
            }
        }

        if (!buttonsMenu)
            return;

        auto practiceSprite =
            cocos2d::CCSprite::createWithSpriteFrameName(
                "GJ_practiceBtn_001.png"
            );

        if (!practiceSprite)
            return;

        practiceSprite->setScale(1.f);

        auto practiceButton = CCMenuItemSpriteExtra::create(
            practiceSprite,
            buttonsMenu,
            menu_selector(ButtonsClass::onReset)
        );

        if (!practiceButton)
            return;

        // Replacement for the removed getChild helper.
        auto getButton = [&](unsigned int index)
            -> CCMenuItemSpriteExtra*
        {
            if (index >= buttonsMenu->getChildrenCount())
                return nullptr;

            return typeinfo_cast<CCMenuItemSpriteExtra*>(
                buttonsMenu->getChildren()->objectAtIndex(index)
            );
        };

        if (buttonsMenu->getChildrenCount() == 2)
        {
            if (auto button = getButton(0))
                button->setPositionX(-100.f);

            if (auto button = getButton(1))
                button->setPositionX(100.f);
        }
        else
        {
            if (auto button = getButton(0))
                button->setPositionX(-130.f);

            if (auto button = getButton(1))
                button->setPositionX(130.f);

            if (auto button = getButton(2))
                button->setPositionX(-45.f);

            practiceButton->setPositionX(45.f);
        }

        practiceButton->setPositionY(-125.f);
        buttonsMenu->addChild(practiceButton);
    }
};
