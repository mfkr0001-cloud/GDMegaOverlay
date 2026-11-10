#include "HidePause.h"

#include <Geode/modify/CCScheduler.hpp>

#include <Geode/Utils.hpp>

#include <Geode/Geode.hpp>

#include "../util.hpp"
#include "../Settings.hpp"

using namespace geode::prelude;

class $modify(CCScheduler)
{
	void update(float dt)
	{
		auto playLayer = GameManager::sharedState()->getPlayLayer();
		auto runningScene = playLayer ? playLayer->getParent() : nullptr;

		if (playLayer)
		{
			playLayer->getChildByID("UILayer")->getChildByID("pause-button-menu")->setVisible(!Settings::get<bool>("general/hide_pause/button"));


#if !defined(__ANDROID__)
			// Desktop-only paused-state field offset.
			if (MBO(bool, playLayer, 0x2F17) && runningScene && runningScene->getChildrenCount() > 1)
				runningScene->getChildByID("PauseLayer")->setVisible(!Settings::get<bool>("general/hide_pause/menu"));
#endif
		}

		CCScheduler::update(dt);
	}
};
