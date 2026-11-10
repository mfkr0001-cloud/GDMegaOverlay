#pragma once

class PlayerObject;
class GJBaseGameLayer;

#include <cocos2d.h>

namespace ShowTrajectory
{
    inline PlayerObject* player1 = nullptr;
    inline PlayerObject* player2 = nullptr;

    inline bool isSimulation = false;
    inline bool simulationDead = true;

    inline bool p1Down = false;
    inline bool p2Down = false;

    inline float frameDt = 0;

    cocos2d::CCDrawNode* getDrawNode();

    void createPlayers();
    void GJBaseGameLayerProcessCommands(GJBaseGameLayer* self);
    void iterateForPlayer(PlayerObject* player, bool pressing, bool isPlayer2);
    void simulationForPlayer(PlayerObject* player, PlayerObject* player2, PlayerObject* playerBase);
    void resetCollisionLog(PlayerObject* self);
    void drawRectangleHitbox(cocos2d::CCDrawNode* node, cocos2d::CCRect const& rect, cocos2d::ccColor4B colBase, cocos2d::ccColor4B colBorder);
    void drawForPlayer(PlayerObject* player);
    
};