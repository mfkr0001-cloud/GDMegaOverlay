#include "ShowTrajectory.h"
#if defined(__ANDROID__)
namespace ShowTrajectory {
cocos2d::CCDrawNode* getDrawNode() { return nullptr; }
void createPlayers() {}
void GJBaseGameLayerProcessCommands(GJBaseGameLayer*) {}
void iterateForPlayer(PlayerObject*, bool, bool) {}
void simulationForPlayer(PlayerObject*, PlayerObject*, PlayerObject*) {}
void resetCollisionLog(PlayerObject*) {}
void drawRectangleHitbox(cocos2d::CCDrawNode*, cocos2d::CCRect const&, cocos2d::ccColor4B, cocos2d::ccColor4B) {}
void drawForPlayer(PlayerObject*) {}
}
#else
#include "ShowTrajectory.h"
#include "util.hpp"
#include "Settings.hpp"
#include "ShowHitboxes.h"
#include "Macrobot/Macrobot.h"

#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/EnhancedGameObject.hpp>
#include <Geode/modify/HardStreak.hpp>

using namespace geode::prelude;
using namespace ShowTrajectory;

cocos2d::CCDrawNode* ShowTrajectory::getDrawNode()
{
    static cocos2d::CCDrawNode* instance = nullptr;
    
    if(!instance)
    {
        instance = cocos2d::CCDrawNode::create();
        instance->retain();
    }

    instance->setVisible(true);

    return instance;
}

void ShowTrajectory::createPlayers()
{
    auto pl = PlayLayer::get();

    player1 = PlayerObject::create(1, 1, pl, pl, true);
    player1->retain();
    player1->setPosition({0, 105});
    player1->setVisible(false);
    pl->m_objectLayer->addChild(player1);

    player2 = PlayerObject::create(1, 1, pl, pl, true);
    player2->retain();
    player2->setPosition({0, 105});
    player2->setVisible(false);
    pl->m_objectLayer->addChild(player2);
}

class $modify(HardStreak)
{
    void addPoint(cocos2d::CCPoint p0)
    {
        if(isSimulation)
            return;
        
        HardStreak::addPoint(p0);
    }
};

class $modify(EffectGameObject)
{
    void triggerObject(GJBaseGameLayer *p0, int p1, const gd::vector<int> *p2)
    {
        //log::debug("is simulation {}", isSimulation);
        if(isSimulation)
            return;
        
        return EffectGameObject::triggerObject(p0, p1, p2);
    }
};

class $modify(GameObject)
{
    void playShineEffect()
    {
        if(isSimulation)
            return;
        
        GameObject::playShineEffect();
    }
};

class $modify(PlayerObject)
{

    void playSpiderDashEffect(cocos2d::CCPoint from, cocos2d::CCPoint to)
    {
        if(isSimulation)
            return;

        PlayerObject::playSpiderDashEffect(from, to);
    }

    void update(float dt)
    {
        PlayerObject::update(dt);
        if(!isSimulation)
            frameDt = dt;
    }

    void ringJump(RingObject *p0, bool p1)
    {
        if(isSimulation)
            return;
        
        PlayerObject::ringJump(p0, p1);
    }
};

class $modify(GJBaseGameLayer)
{

    bool canBeActivatedByPlayer(PlayerObject* p0, EffectGameObject* p1)
    {
        if(isSimulation)
            return false;
        
        return GJBaseGameLayer::canBeActivatedByPlayer(p0, p1);
    }

    void handleButton(bool p0, int p1, bool p2)
    {
        if(p2 && p1 == 1)
            p1Down = p0;
        else if(p1 == 1)
            p2Down = p0;

        GJBaseGameLayer::handleButton(p0, p1, p2);
    }

    void collisionCheckObjects(PlayerObject* p0, gd::vector<GameObject*>* vec, int p2, float p3)
    {
        if (isSimulation) 
        {
            gd::vector<GameObject*> extra = *vec;
            
            auto new_end = std::remove_if(vec->begin(), vec->end(), [&](GameObject* p1) {
                bool result = p1->m_objectType != GameObjectType::Solid &&
                            p1->m_objectType != GameObjectType::Hazard &&
                            p1->m_objectType != GameObjectType::AnimatedHazard &&
                            p1->m_objectType != GameObjectType::Slope;
                return result;
            });

            vec->erase(new_end, vec->end());
            p2 = vec->size();

            GJBaseGameLayer::collisionCheckObjects(p0, vec, p2, p3);

            *vec = extra;
            return;
        }

        GJBaseGameLayer::collisionCheckObjects(p0, vec, p2, p3);
    }

    void playerTouchedRing(PlayerObject* player, RingObject* ring)
    {
        if(isSimulation)
            return;

        GJBaseGameLayer::playerTouchedRing(player, ring);
    }

    void updateCamera(float dt)
    {
        getDrawNode()->setVisible(false);
    
        if(!PlayLayer::get() || !Settings::get<bool>("player/show_trajectory/enabled", false))
            return GJBaseGameLayer::updateCamera(dt);
        
        isSimulation = true;

        getDrawNode()->setVisible(true);
        getDrawNode()->clear();
        MBO(bool, getDrawNode(), 0x135) = false; //idk what this is but without it segments dont draw on online levels

        simulationForPlayer(player1, player2, this->m_player1);

        if(MBO(bool, this, 878))
            simulationForPlayer(player1, player2, this->m_player2);

        isSimulation = false;

        GJBaseGameLayer::updateCamera(dt);
    }
};

class $modify(PlayLayer)
{

#if !defined(__ANDROID__)
bool init(GJGameLevel* level, bool unk1, bool unk2)
{
    player1 = nullptr;
    player2 = nullptr;
	bool res = PlayLayer::init(level, unk1, unk2);
	createPlayers();
    this->m_debugDrawNode->getParent()->addChild(getDrawNode());
    getDrawNode()->setZOrder(this->m_debugDrawNode->getZOrder());
	return res;
}
#endif

void destroyPlayer(PlayerObject *p0, GameObject *p1)
{
    if(isSimulation && (p0 == player1 || p0 == player2))
    {
        simulationDead = true;
        return;
    }
    
    PlayLayer::destroyPlayer(p0, p1);
}

void incrementJumps()
{
    if(isSimulation)
        return;
    
    PlayLayer::incrementJumps();
}

void playEndAnimationToPos(cocos2d::CCPoint p0)
{
    if(isSimulation)
        return;
    
    PlayLayer::playEndAnimationToPos(p0);
}

void flipGravity(PlayerObject *p0, bool p1, bool p2)
{
    if(isSimulation)
        return;
    
    PlayLayer::flipGravity(p0, p1, p2);
}

};

void ShowTrajectory::drawRectangleHitbox(cocos2d::CCDrawNode* node, cocos2d::CCRect const& rect, ccColor4B colBase, ccColor4B colBorder)
{
	constexpr size_t N = 4;
	std::vector<cocos2d::CCPoint> points(N);

	points[0] = CCPointMake(rect.getMinX(), rect.getMinY());
	points[1] = CCPointMake(rect.getMinX(), rect.getMaxY());
	points[2] = CCPointMake(rect.getMaxX(), rect.getMaxY());
	points[3] = CCPointMake(rect.getMaxX(), rect.getMinY());

	node->drawPolygon(const_cast<cocos2d::CCPoint*>(points.data()), points.size(), ccc4FFromccc4B(colBase),
						0.25, ccc4FFromccc4B(colBorder));
}

void ShowTrajectory::drawForPlayer(PlayerObject* player)
{
	if (!player)
		return;

	CCRect* rect1 = reinterpret_cast<CCRect*(__thiscall*)(GameObject*)>(base::get() + 0x13a570)(player);
	CCRect rect2 = player->m_vehicleSize >= 1.f ? ShowHitboxes::getObjectRect(*rect1, 0.25f, 0.25f) : ShowHitboxes::getObjectRect(*rect1, 0.4f, 0.4f);
	drawRectangleHitbox(getDrawNode(), *rect1, ccColor4B(255, 0, 0, 0), ccColor4B(255, 0, 0, 255));
	drawRectangleHitbox(getDrawNode(), rect2, ccColor4B(0, 255, 0, 0), ccColor4B(0, 255, 0, 255));
}

void ShowTrajectory::resetCollisionLog(PlayerObject* self)
{
    self->m_unk4e8->removeAllObjects();
    self->m_unk4ec->removeAllObjects();
    self->m_unk4f0->removeAllObjects();
    self->m_unk4f4->removeAllObjects();
}

void ShowTrajectory::iterateForPlayer(PlayerObject* player, bool pressing, bool isPlayer2)
{
    player->setVisible(false);
    simulationDead = false;

    bool iterAction = false;
    
    size_t iterations = 300;
    float dt = frameDt;
    
    if(Settings::get<bool>("player/show_trajectory/performance_mode", false))
    {
        iterations /= 2;
        dt *= 2.f;
    }

    for(int i = 0; i < iterations; i++)
    {
        CCPoint playerPrevPos = player->getPosition();
        
        //resetCollisionLog
        //reinterpret_cast<void(__thiscall *)(PlayerObject *)>(base::get() + 0x2C8080)(player);
        resetCollisionLog(player);
        
        PlayLayer::get()->checkCollisions(player, dt, false);

        if(simulationDead)
            break;
        
        if(!iterAction && (!isPlayer2 && !p1Down || isPlayer2 && !p2Down))
        {
            //press the button here. this is in order to replicate the lower jump height of the first jump. if you call this before it will be a full jump. silly robtop
            iterAction = true;
            pressing ? player->pushButton(PlayerButton::Jump) : player->releaseButton(PlayerButton::Jump);
        }

        player->update(dt);
        player->updateSpecial(dt);
        player->updateRotation(dt);

        getDrawNode()->drawSegment(playerPrevPos, player->getPosition(), 0.65f, pressing ? ccColor4F(0.f, 1.f, 0.1f, 1.f) : ccColor4F(1.f, 0.f, 0.1f, 1.f));
    }

    drawForPlayer(player);
}

void ShowTrajectory::simulationForPlayer(PlayerObject* player, PlayerObject* player2, PlayerObject* playerBase)
{
    if(!player || !playerBase)
        return;

    bool isPlayer2 = playerBase == PlayLayer::get()->m_player2;

    player->spawnFromPlayer(playerBase, false);
    player->m_gravityMod = playerBase->m_gravityMod;

    if(isPlayer2 && p2Down || !isPlayer2 && p1Down)
        player->pushButton(PlayerButton::Jump);

    if(playerBase->m_isOnGround)
        player->m_isOnGround = true;

    iterateForPlayer(player, true, isPlayer2);

    player2->spawnFromPlayer(playerBase, false);

    if(isPlayer2 && p2Down || !isPlayer2 && p1Down)
        player->releaseButton(PlayerButton::Jump);

    if(playerBase->m_isOnGround)
        player2->m_isOnGround = true;

    iterateForPlayer(player2, false, isPlayer2);
}
#endif // !__ANDROID__
