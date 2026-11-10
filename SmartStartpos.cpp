#include "Settings.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

std::vector<StartPosObject*> startPositions;
std::vector<GameObject*> dualPortals, gamemodePortals, miniPortals, speedChanges, mirrorPortals;

bool compareXPos(GameObject* a, GameObject* b) {
    return a->getPositionX() < b->getPositionX();
}

GameObject* getClosestObject(std::vector<GameObject*>& vec, StartPosObject* startPos)
{
    GameObject* closest = nullptr;

    std::sort(vec.begin(), vec.end(), compareXPos);

    for(auto obj : vec)
    {
        if(obj->getPositionX() - 10 > startPos->getPositionX())
            break;
        else if(obj->getPositionX() - 10 < startPos->getPositionX())
            closest = obj;
    }

    return closest;
}

void setupStartPos(StartPosObject* startPos)
{
    LevelSettingsObject* startPosSettings = (LevelSettingsObject*)startPos->m_unknown;
    LevelSettingsObject* levelSettings = PlayLayer::get()->m_levelSettings;

    startPosSettings->m_startDual = levelSettings->m_startDual;
    startPosSettings->m_startMode = levelSettings->m_startMode;
    startPosSettings->m_startMini = levelSettings->m_startMini;
    startPosSettings->m_startSpeed = levelSettings->m_startSpeed;

    GameObject* obj = getClosestObject(dualPortals, startPos);
    if(obj)
        startPosSettings->m_startDual = obj->m_objectID == 286;

    obj = getClosestObject(gamemodePortals, startPos);
    if(obj)
    {
        switch(obj->m_objectID)
        {
            case 12:
				startPosSettings->m_startMode = 0;
				break;
			case 13:
				startPosSettings->m_startMode = 1;
				break;
			case 47:
				startPosSettings->m_startMode = 2;
				break;
			case 111:
				startPosSettings->m_startMode = 3;
				break;
			case 660:
				startPosSettings->m_startMode = 4;
				break;
			case 745:
				startPosSettings->m_startMode = 5;
				break;
			case 1331:
				startPosSettings->m_startMode = 6;
				break;
            case 1933:
				startPosSettings->m_startMode = 7;
				break;
        }
    }

    obj = getClosestObject(miniPortals, startPos);
    if(obj)
        startPosSettings->m_startMini = obj->m_objectID == 101;

    obj = getClosestObject(speedChanges, startPos);
    if(obj)
    {
        switch(obj->m_objectID)
        {
            case 200:
				startPosSettings->m_startSpeed = Speed::Slow;
				break;
			case 201:
				startPosSettings->m_startSpeed = Speed::Normal;
				break;
			case 202:
				startPosSettings->m_startSpeed = Speed::Fast;
				break;
			case 203:
				startPosSettings->m_startSpeed = Speed::Faster;
				break;
			case 1334:
				startPosSettings->m_startSpeed = Speed::Fastest;
				break;
        }
    }
}

class $modify(PlayLayer)
{
    bool init(GJGameLevel* level, bool unk1, bool unk2)
    {
        dualPortals.clear();
        gamemodePortals.clear();
        miniPortals.clear();
        miniPortals.clear();
        speedChanges.clear();
        mirrorPortals.clear();
        startPositions.clear();

        return PlayLayer::init(level, unk1, unk2);
    }

    void resetLevel()
    {
        if(Settings::get<bool>("level/smart_startpos", false))
        {
            for(StartPosObject* obj : startPositions)
                setupStartPos(obj);
        }

        PlayLayer::resetLevel();
    }

    void addObject(GameObject* obj)
    {
        PlayLayer::addObject(obj);

        switch(obj->m_objectID)
        {
            case 31:
                startPositions.push_back((StartPosObject*)obj);
                break;
			case 12:
			case 13:
			case 47:
			case 111:
			case 660:
			case 745:
			case 1331:
            case 1933:
				gamemodePortals.push_back(obj);
				break;
			case 45:
			case 46:
				mirrorPortals.push_back(obj);
				break;
			case 99:
			case 101:
				miniPortals.push_back(obj);
				break;
			case 286:
			case 287:
				dualPortals.push_back(obj);
				break;
			case 200:
			case 201:
			case 202:
			case 203:
			case 1334:
				speedChanges.push_back(obj);
				break;
        }
    }
};