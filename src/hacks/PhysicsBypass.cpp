#include "PhysicsBypass.h"
#include "Settings.hpp"
#include "Macrobot/Macrobot.h"
#include <Geode/modify/PlayLayer.hpp>
#include "Common.h"

class $modify(PlayLayer)
{
	bool init(GJGameLevel* p0, bool p1, bool p2)
	{
		// A level pointer should normally be valid here, but never dereference it
		// without checking. Keep the original timestamp for later tickrate work.
		PhysicsBypass::timestamp = p0 ? p0->m_timestamp : 0;

		return PlayLayer::init(p0, p1, p2);
	}

	void resetLevel()
	{
		PlayLayer::resetLevel();
		PhysicsBypass::calculateTickrate();
	}
};

void PhysicsBypass::calculateTickrate()
{
#if defined(__ANDROID__)
	return;
#else
	float tps = Common::getTPS();
	util::Write<float>(base::get() + 0x49D548, 1.f / tps);
	
	/* if (tps == 240.f)
		return; */
	
	if (PlayLayer::get() && PlayLayer::get()->m_level->m_timestamp > 0)
	{
		float timestampMultiplier = (tps / 240.f);
		float stepsMultiplier = (timestamp * timestampMultiplier) / PlayLayer::get()->m_level->m_timestamp;
		PlayLayer::get()->m_level->m_timestamp = timestamp * timestampMultiplier;
		PlayLayer::get()->m_gameState.m_unk1f8 *= stepsMultiplier;
	}
#endif
}
