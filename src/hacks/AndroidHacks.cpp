// Android implementations of hacks that the desktop build applies as raw memory
// patches (res/hacks/*.json). Desktop addresses are invalid on Android ARM64, so
// these use Geode hooks instead. Each hook is gated by the same checkbox setting
// ("patch/<name>") that the menu already uses, so the UI needs no extra state.
//
// The hook points follow the approach used by the GDH mod (TobyAdd) for
// Geometry Dash 2.2081. Keep the entries here in sync with
// JsonPatches::androidImplemented in JsonPatches.cpp.
// Android hook implementations for the Bypass entries listed in JsonPatches.cpp.
#if defined(__ANDROID__)

#include "../Settings.hpp"

#include <array>
#include <cstring>
#include <string>
#include <unordered_map>

#include <Geode/Geode.hpp>
#include <Geode/modify/CCTextInputNode.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/LevelAreaInnerLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

namespace
{
    bool hackOn(const char* name)
    {
        // peek, а не get: хуки вызываются очень часто, и они не должны
        // создавать записи в сохранениях во время игры.
        return Settings::peek<bool>(std::string("patch/") + name, false);
    }
}

// ---------------------------------------------------------------- Bypass

class $modify(AndroidTextInputNode, CCTextInputNode)
{
    void updateLabel(gd::string str)
    {
        if (hackOn("Text Length"))
            this->setMaxLabelLength(9999);

        if (hackOn("Character Filter"))
            this->setAllowedChars(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
                "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~ ");

        CCTextInputNode::updateLabel(std::move(str));
    }
};

class $modify(AndroidTreasureRoomGameManager, GameManager)
{
    bool getUGV(char const* key)
    {
        if (GameManager::getUGV(key))
            return true;

        // "5" is the Treasure Room unlock flag. Compare the key first: this
        // function is called very often.
        return key && std::strcmp(key, "5") == 0 && hackOn("Treasure Room");
    }
};

class $modify(AndroidMainLevelsManager, GameLevelManager)
{
    GJGameLevel* getMainLevel(int levelID, bool dontGetLevelString)
    {
        auto* level = GameLevelManager::getMainLevel(levelID, dontGetLevelString);
        if (level && level->m_requiredCoins > 0 && hackOn("Main Levels"))
            level->m_requiredCoins = 0;
        return level;
    }
};

class $modify(AndroidMainLevelsArea, LevelAreaInnerLayer)
{
    bool init(bool returning)
    {
        if (!hackOn("Main Levels"))
            return LevelAreaInnerLayer::init(returning);

        static constexpr std::array<int, 4> ids = {5001, 5002, 5003, 5004};
        auto* stats = GameStatsManager::get();

        // Pretend the tower levels are completed only while the layer is built,
        // then restore the real completion state.
        std::unordered_map<int, bool> wasCompleted;
        for (int id : ids)
        {
            wasCompleted[id] = stats->hasCompletedMainLevel(id);
            stats->m_completedLevels->setObject(
                CCBool::create(true), fmt::format("n_{}", id));
        }

        const bool ok = LevelAreaInnerLayer::init(returning);

        for (auto const& [id, completed] : wasCompleted)
            if (!completed)
                stats->m_completedLevels->removeObjectForKey(fmt::format("n_{}", id));

        return ok;
    }
};

// ---------------------------------------------------------------- Player

class $modify(AndroidCheckpointLimitPlayLayer, PlayLayer)
{
    void storeCheckpoint(CheckpointObject* checkpoint)
    {
        if (!hackOn("Checkpoint Limit Bypass") || !checkpoint || !m_checkpointArray ||
            !checkpoint->m_physicalCheckpointObject)
            return PlayLayer::storeCheckpoint(checkpoint);

        // The original removes the oldest checkpoint after the 50th; skip that.
        m_checkpointArray->addObject(checkpoint);
        PlayLayer::addToSection(checkpoint->m_physicalCheckpointObject);
    }
};

class $modify(AndroidPlayerEffects, PlayerObject)
{
    void playDeathEffect()
    {
        if (!hackOn("No Death Effect"))
            PlayerObject::playDeathEffect();
    }

    void playSpawnEffect()
    {
        if (!hackOn("No Respawn Blink"))
            PlayerObject::playSpawnEffect();
    }

    void playSpiderDashEffect(CCPoint from, CCPoint to)
    {
        if (!hackOn("No Spider Dash"))
            PlayerObject::playSpiderDashEffect(from, to);
    }

    void toggleRobotMode(bool enable, bool noEffects)
    {
        PlayerObject::toggleRobotMode(enable, noEffects);

        if (hackOn("No Robot Fire"))
        {
            if (m_robotFire)
                m_robotFire->setOpacity(0);
            if (m_robotBurstParticles)
                m_robotBurstParticles->setOpacity(0);
        }
    }
};

#endif // __ANDROID__
