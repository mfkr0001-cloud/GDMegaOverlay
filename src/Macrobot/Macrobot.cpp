#include <filesystem>
#include <algorithm>
#include "../Common.h"
#include "../ConstData.h"
#include "../GUI/GUI.h"
#include "../hacks/ShowTrajectory.h"

#include "../util.hpp"
#include "AudioRecord.h"
#include "../Settings.hpp"

#include <fstream>
#include <imgui-cocos.hpp>
#include <iostream>

#include "Macrobot.h"
#include "Clickpacks.h"
#include "../hacks/PhysicsBypass.h"
#include "Macrobot/MacroEditor.h"

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CheckpointObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/binding/GameManager.hpp>

using namespace geode::prelude;
using namespace Macrobot;

#if defined(__ANDROID__)
static void stopAndroidPlayback();
#endif

class $modify(CCKeyboardDispatcher)
{
	bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool arr, double time)
	{
		if (key == enumKeyCodes::KEY_D || key == enumKeyCodes::KEY_ArrowRight)
			direction = down ? 1 : 0;

		if (key == enumKeyCodes::KEY_A || key == enumKeyCodes::KEY_ArrowLeft)
			direction = down ? -1 : 0;
		
		int advanceKey = Settings::get<int>("macrobot/frame_step/key", ImGuiKey_G);

		int convertedKey = ConvertKeyEnum(key);

		if(convertedKey == advanceKey)
			holdingAdvance = down;

		if (!down || arr)
			return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, arr, time);

		if (convertedKey == advanceKey && PlayLayer::get())
			targetSteps = currentFrame + Settings::get<int>("macrobot/frame_step/steps", 1);

		return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, arr, time);
	}
};

class $modify(PlayLayer)
{
    void onQuit()
    {
        // Stop the active Macro Bot mode before leaving the level.
        // This project uses playerMode/macro, not a separate `recorder` object.
        if (playerMode == RECORDING)
            playerMode = DISABLED;
#if !defined(__ANDROID__)
        checkpoints.clear();
#endif
#if defined(__ANDROID__)
        androidCheckpointFrames.clear();
        stopAndroidPlayback();
#endif
        playerMode = DISABLED;
        PlayLayer::onQuit();
    }

    void onExit()
    {
        botInput = false;
        PlayLayer::onExit();
    }

#if defined(__ANDROID__)
    // Practice mode: when the game respawns the player at a checkpoint, the macro
    // must continue from the checkpoint's frame instead of being wiped. The game
    // itself restores the player state; we only keep our frame counter and the
    // recorded inputs in sync with it.
    void loadFromCheckpoint(CheckpointObject* checkpoint)
    {
        const auto found = androidCheckpointFrames.find(checkpoint);
        if (playerMode == DISABLED || found == androidCheckpointFrames.end())
        {
            // Unknown checkpoint (or bot off): behave like a normal reset.
            resetFromStart = true;
            PlayLayer::loadFromCheckpoint(checkpoint);
            return;
        }

        resetFromStart = false;
        const uint32_t checkpointFrame = found->second;

        PlayLayer::loadFromCheckpoint(checkpoint);

        currentFrame = checkpointFrame;
        if (playerMode == RECORDING)
        {
            // Drop everything recorded after the checkpoint (the attempt we just lost).
            macro.inputs.erase(
                std::remove_if(macro.inputs.begin(), macro.inputs.end(),
                    [checkpointFrame](const Action& action) { return action.frame > checkpointFrame; }),
                macro.inputs.end());
            recordingHeld1.clear();
            recordingHeld2.clear();
        }
        else
        {
            // Playback: continue with the first input at or after this frame.
            actionIndex = 0;
            while (actionIndex < macro.inputs.size() && macro.inputs[actionIndex].frame < checkpointFrame)
                ++actionIndex;
            playbackHeld1.clear();
            playbackHeld2.clear();
        }
    }
#endif

#if !defined(__ANDROID__)
    void loadFromCheckpoint(CheckpointObject* checkpoint)
    {
        resetFromStart = false;
        if (checkpoints.contains(checkpoint) && playerMode != DISABLED && PlayLayer::get())
        {
            CheckpointData checkpointData = checkpoints[checkpoint];

            if (playerMode == RECORDING)
            {
                const auto check = [&](const Action& action) -> bool { return action.frame > checkpointData.frame; };
                macro.inputs.erase(std::remove_if(macro.inputs.begin(), macro.inputs.end(), check), macro.inputs.end());
            }

            PlayLayer::loadFromCheckpoint(checkpoint);
            *(int*)((char*)base::get() + 0x4F24A8) = checkpointData.randomSeed;
            checkpointData.p1.apply(this->m_player1, true);
            if (MBO(bool, this, 878))
                checkpointData.p2.apply(this->m_player2, true);
        }
        else
            PlayLayer::loadFromCheckpoint(checkpoint);
    }
#endif

    void resetLevel()
    {
#if defined(__ANDROID__)
        // loadFromCheckpoint (hooked above) clears this flag when the game
        // respawns at a practice checkpoint during resetLevel.
        resetFromStart = true;
        PlayLayer::resetLevel();

        if (resetFromStart)
        {
            currentFrame = 0;
            actionIndex = 0;
            androidCheckpointFrames.clear();
            playbackHeld1.clear();
            playbackHeld2.clear();
            recordingHeld1.clear();
            recordingHeld2.clear();
            if (playerMode == RECORDING)
            {
                macro.inputs.clear();
                macro.framerate = std::max(1.f, Common::getTPS());
                if (auto* level = PlayLayer::get() ? PlayLayer::get()->m_level : nullptr)
                {
                    macro.levelInfo.id = level->m_levelID.value();
                    macro.levelInfo.name = level->m_levelName;
                }
            }
        }
        else if (playerMode == RECORDING)
        {
            // Respawned at a checkpoint: make sure no button stays held from the
            // lost attempt by recording a release at the checkpoint frame.
            recordAction(PlayerButton::Jump, currentFrame, false, true);
            if (this->m_levelSettings->m_twoPlayerMode)
                recordAction(PlayerButton::Jump, currentFrame, false, false);
        }
        return;
#else
        holdingAdvance = false;
        targetSteps = 0;

        if (playerMode != DISABLED)
        {
            actionIndex = 0;
            resetFromStart = true;
            resetFrame = true;
            downForKey1.clear();
            downForKey2.clear();
            timeForKey1.clear();
            timeForKey2.clear();
            PlayLayer::resetLevel();

            GJGameLevel* level = PlayLayer::get()->m_level;
            macro.levelInfo.id = level->m_levelID.value();
            macro.levelInfo.name = level->m_levelName;

            if (resetFromStart)
            {
                if (playerMode == RECORDING)
                {
                    checkpoints.clear();
                    macro.inputs.clear();
                    macro.framerate = Common::getTPS();
                    macro.seed = *(int*)((char*)base::get() + 0x4F24A8);
                }
                else
                    *(int*)((char*)base::get() + 0x4F24A8) = macro.seed;
            }

            resetFrame = false;
            if (this->m_player1->m_isPlatformer)
            {
                if ((ImGui::IsKeyDown(ImGuiKey_RightArrow) || ImGui::IsKeyDown(ImGuiKey_D)) &&
                    !(ImGui::IsKeyDown(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_A)))
                    this->m_player1->pushButton(PlayerButton::Right);
                else if (!(ImGui::IsKeyDown(ImGuiKey_RightArrow) || ImGui::IsKeyDown(ImGuiKey_D)) &&
                         (ImGui::IsKeyDown(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_A)))
                    this->m_player1->pushButton(PlayerButton::Left);
            }

            if (playerMode == RECORDING)
            {
                bool isDual = MBO(bool, this, 878);
                Macrobot::recordAction(PlayerButton::Jump, this->m_gameState.m_unk1f8 + 1, false, true);
                if (isDual)
                    Macrobot::recordAction(PlayerButton::Jump, this->m_gameState.m_unk1f8 + 1, false, false);
            }
        }
        else
            PlayLayer::resetLevel();
#endif
    }
};

class $modify(PlayerObject)
{
	void pushButton(PlayerButton btn)
	{
		PlayerObject::pushButton(btn);

		if(ShowTrajectory::isSimulation)
			return;
		
		auto pl = PlayLayer::get();

		if (pl && playerMode == RECORDING && !resetFrame)
		{
			if (!pl->m_levelSettings->m_twoPlayerMode && this == pl->m_player2)
				return;

			recordAction(btn, currentFrame, true, this == pl->m_player1);
#if defined(__ANDROID__)
			(this == pl->m_player1 ? recordingHeld1 : recordingHeld2)[(int)btn] = true;
#endif
		}
	}

	void releaseButton(PlayerButton btn)
	{
		PlayerObject::releaseButton(btn);

		if(ShowTrajectory::isSimulation)
			return;

		auto pl = PlayLayer::get();

		if (pl && playerMode == RECORDING && !resetFrame)
		{
			if (!pl->m_levelSettings->m_twoPlayerMode && this == pl->m_player2)
				return;

			if (btn == PlayerButton::Right && direction == 1)
				return;

			if (btn == PlayerButton::Left && direction == -1)
				return;

			recordAction(btn, currentFrame, false, this == pl->m_player1);
#if defined(__ANDROID__)
			(this == pl->m_player1 ? recordingHeld1 : recordingHeld2)[(int)btn] = false;
#endif
		}
	}
};

Macrobot::Action* Macrobot::recordAction(PlayerButton key, uint32_t frame, bool press, bool player1)
{
	Action ac(frame, (int)key, !player1, press);

	PlayLayer* pl = PlayLayer::get();
	PlayerObject* player = player1 ? pl->m_player1 : pl->m_player2;

#if !defined(__ANDROID__)
	if ((CorrectionType)Settings::get<int>("macrobot/corrections") == ACTION && (player->m_position.x != 0 && player->m_position.y != 0))
	{
		ac.correction = Correction(frame, !player1);
		ac.correction->checkpoint.fromPlayer(player, false);
	}
#endif

	macro.inputs.push_back(ac);

	return &macro.inputs[macro.inputs.size() - 1];
}

class $modify(GJBaseGameLayer)
{
	void update(float dt)
	{
#if defined(__ANDROID__)
		// Apply inputs at the beginning of their recorded game frame, before
		// the physics update for that frame runs.
		if (playerMode == PLAYBACK && PlayLayer::get()) {
			while (actionIndex < macro.inputs.size() && currentFrame >= macro.inputs[actionIndex].frame) {
				handleAction(macro.inputs[actionIndex]);
				++actionIndex;
			}
		}
		GJBaseGameLayer::update(dt);
		++currentFrame;
		if (playerMode == PLAYBACK && actionIndex >= macro.inputs.size()) {
			if (auto* pl = PlayLayer::get()) {
				botInput = true;
				for (const auto& [button, down] : playbackHeld1)
					if (down) pl->handleButton(false, button, true);
				for (const auto& [button, down] : playbackHeld2)
					if (down) pl->handleButton(false, button, false);
				botInput = false;
			}
			playbackHeld1.clear();
			playbackHeld2.clear();
			playerMode = DISABLED;
			actionIndex = 0;
		}
		return;
#else
		if (!Settings::get<bool>("macrobot/frame_step/enabled", false) || !PlayLayer::get())
		{
			GJBaseGameLayer::update(dt);
			return;
		}

		float speedhack = Settings::get<bool>("general/speedhack/enabled") ? Settings::get<float>("general/speedhack/value") : 1.f;

		if (Settings::get<bool>("macrobot/frame_step/hold", false) && holdingAdvance)
			advanceHoldTime += dt / speedhack;
		else
			advanceHoldTime = 0.0f;

		if (PlayLayer::get()->m_gameState.m_unk1f8 < targetSteps)
			GJBaseGameLayer::update(dt);

		if (advanceHoldTime > 0.5)
			targetSteps = PlayLayer::get()->m_gameState.m_unk1f8 + 1;
#endif
	}


	void handleButton(bool down, int button, bool player1)
	{
		if ((playerMode == PLAYBACK && !botInput) || resetFrame)
			return;
		
		GJBaseGameLayer::handleButton(down, button, player1);
	}
};

#if defined(__ANDROID__)
class $modify(CheckpointObject)
{
	bool init()
	{
		const bool res = CheckpointObject::init();

		// Remember the macro frame at which each checkpoint was created.
		if (playerMode != DISABLED && PlayLayer::get() && currentFrame > 0)
			androidCheckpointFrames[this] = currentFrame;

		return res;
	}
};
#endif // __ANDROID__

#if !defined(__ANDROID__)
class $modify(CheckpointObject)
{
	bool init()
	{
		bool res = CheckpointObject::init();

		if (playerMode != DISABLED && PlayLayer::get() && PlayLayer::get()->m_gameState.m_unk1f8 > 0)
		{
			CheckpointData data;
			data.frame = PlayLayer::get()->m_gameState.m_unk1f8;
			data.randomSeed = *(int*)((char*)base::get() + 0x4F24A8);
			data.p1.fromPlayer(PlayLayer::get()->m_player1, true);
			data.p2.fromPlayer(PlayLayer::get()->m_player2, true);

			checkpoints[this] = data;
		}

		return res;
	}
};
#endif // !__ANDROID__

void Macrobot::handleAction(Action& action)
{
	auto pl = PlayLayer::get();

	int button = action.button;
	bool down = action.down;
	bool player1 = !action.player2;

	botInput = true;
	pl->handleButton(down, button, player1);
	botInput = false;
#if defined(__ANDROID__)
	(player1 ? playbackHeld1 : playbackHeld2)[button] = down;
#endif

	if ((CorrectionType)Settings::get<int>("macrobot/corrections") == ACTION && action.correction.has_value())
		action.correction->checkpoint.apply(action.correction->player2 ? pl->m_player2 : pl->m_player1, false);

	if (!Settings::get<bool>("macrobot/clicks/enabled", false))
		return;

	float timestamp = action.frame / macro.framerate;

	std::unordered_map<int, bool> &downForKey = player1 ? downForKey1 : downForKey2;
	std::unordered_map<int, float> &timeForKey = player1 ? timeForKey1 : timeForKey2;

	if (downForKey.contains(button) && downForKey[button] == down)
		return;

	if (!timeForKey.contains(button))
		timeForKey[button] = 0;

	downForKey[button] = down;

	float timeDifference = timestamp - timeForKey[button];
	float softclickTime = Settings::get<float>("clickpacks/softclicks_at", 0.1f);
	float minPitch = Settings::get<float>("clickpacks/click/min_pitch", 0.98f);
	float maxPitch = Settings::get<float>("clickpacks/click/max_pitch", 1.02f);
	float volume = Settings::get<float>("clickpacks/click/volume", 1.f);
	float pitchVar = minPitch +
					static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (maxPitch - minPitch)));

	FMOD::Sound* soundToPlay = nullptr;
	FMOD::System* system = FMODAudioEngine::sharedEngine()->m_system;

	Clickpack& clickpack = player1 ? Clickpacks::currentClickpack : Clickpacks::currentClickpackP2;

	switch(button)
	{
		case 1:
			soundToPlay = down ? (timeDifference <= softclickTime ? clickpack.randomSoftClick() : clickpack.randomClick()) : clickpack.randomRelease();
			break;
		case 2:
		case 3:
			soundToPlay = down ? clickpack.randomPlatClick() : clickpack.randomPlatRelease();
	}

	timeForKey[button] = timestamp;

	system->playSound(soundToPlay, nullptr, false, &clickChannel);
	clickChannel->setPitch(pitchVar);
	clickChannel->setVolume(volume);
}

void Macrobot::GJBaseGameLayerProcessCommands(GJBaseGameLayer* self)
{
#if defined(__ANDROID__)
	(void)self;
	return;
#else
	reinterpret_cast<void(__thiscall *)(GJBaseGameLayer *)>(base::get() + 0x1BD240)(self);

	if (playerMode == PLAYBACK && PlayLayer::get())
	{
		uint32_t gameFrame = self->m_gameState.m_unk1f8;

		while (actionIndex < macro.inputs.size() && (gameFrame >= macro.inputs[actionIndex].frame))
		{
			handleAction(macro.inputs[actionIndex]);
			actionIndex++;
		}
	}
#endif
}

$execute
{
#if !defined(__ANDROID__)
	Mod::get()->hook(reinterpret_cast<void *>(base::get() + 0x1BD240), &GJBaseGameLayerProcessCommands, "GJBaseGameLayer::processCommands", tulip::hook::TulipConvention::Thiscall);
#endif
}

void Macrobot::MPlayerCheckpoint::fromPlayer(PlayerObject *player, bool fullCapture)
{
#if defined(__ANDROID__)
	(void)player;
	(void)fullCapture;
	return;
#else
	// playerObject + 2280 isplatformer
	// playerObject + 2160 xVelPlatformer
	if (!player)
		return;

	this->yVel = player->m_yVelocity;
	this->rotation = player->getRotation();
	this->xVel = player->m_platformerXVelocity;
	this->xPos = player->m_position.x;
	this->yPos = player->m_position.y;
	this->rotationRate = player->m_rotationSpeed;
	this->lastSnappedTo = player->m_objectSnappedTo;
	this->lastSnappedTo2 = MBO(GameObject*, player, 1724);
	this->isOnSlope = player->m_isOnSlope;
	this->wasOnSlope = player->m_wasOnSlope;
	this->isOnGround = player->m_isOnGround;

	if (fullCapture)
	{
		this->randomProperties.reserve(2268);
		for (int i = 0; i < 2265; i++)
			this->randomProperties.push_back(MBO(float, player, 160 + i));
	}
#endif
}

void Macrobot::MPlayerCheckpoint::apply(PlayerObject* player, bool fullRestore)
{
#if defined(__ANDROID__)
	(void)player;
	(void)fullRestore;
	return;
#else
	if (PlayLayer::get()->m_gameState.m_unk1f8 <= 0)
		return;

	if (fullRestore)
	{
		//this range contains a value that makes orb y vel work, if it doesnt work on future gd updates change this
		for (int i = 1410; i < 1700; i++)
		{
			if (this->randomProperties[i] < 10000 && this->randomProperties[i] > -10000)
			{
				*reinterpret_cast<float *>(reinterpret_cast<uintptr_t>(player) + 160 + i) = this->randomProperties[i];
			}
		}

		for (int i = 1794; i < 2265; i++)
		{
			if (this->randomProperties[i] < 10000 && this->randomProperties[i] > -10000)
			{
				*reinterpret_cast<float *>(reinterpret_cast<uintptr_t>(player) + 160 + i) = this->randomProperties[i];
			}
		}

		//this first range contains a value that makes the player turn big if its < 100? tf
		for (int i = 100; i < 1300; i++)
		{
			if (this->randomProperties[i] < 10000 && this->randomProperties[i] > -10000)
			{
				*reinterpret_cast<float *>(reinterpret_cast<uintptr_t>(player) + 160 + i) = this->randomProperties[i];
			}
		}

		player->m_objectSnappedTo = this->lastSnappedTo;
		MBO(GameObject*, player, 1724) = this->lastSnappedTo2; // get this in checkjumpsnaptoobject
	}

	player->m_yVelocity = this->yVel;
	player->setRotation(this->rotation);

	player->setPositionX(this->xPos);
	player->setPositionY(this->yPos);

	player->m_position = cocos2d::CCPoint(this->xPos, this->yPos);

	player->m_platformerXVelocity = this->xVel;

	player->m_rotationSpeed = this->rotationRate;

	player->m_isOnSlope = this->isOnSlope;
	player->m_wasOnSlope = this->wasOnSlope;
	player->m_isOnGround = this->isOnGround;

	//TODO: spamming checkpoints on slopes changes slope start time

	/* double slopeTime = MBO(double, self->m_player1, 2144);
	double slopeStartTime = MBO(double, self->m_player1, 1240);*/
#endif
}

void Macrobot::save(const std::string& file)
{
	if (macro.inputs.size() <= 0)
	{
		Common::showWithPriority(FLAlertLayer::create("Error", "No inputs in macro!", "Ok"));
		return;
	}

	if (file == "")
	{
		Common::showWithPriority(FLAlertLayer::create("Error", "Macro name is empty!", "Ok"));
		return;
	}

	std::ofstream f(Mod::get()->getSaveDir() / "macros" / (file + ".gdr"), std::ios::binary);

	if (!f)
	{
		Common::showWithPriority(FLAlertLayer::create("Error", "Could not save macro!\n" + (Mod::get()->getSaveDir() / "macros" / (file + ".gdr")).string() + "!", "Ok"));
		f.close();
		return;
	}

	macro.author = GJAccountManager::sharedState()->m_username;
	macro.description = macroDescription;
	macro.duration = macro.inputs[macro.inputs.size() - 1].frame / macro.framerate;
	macro.gameVersion = 2.204f;
	macro.version = 1.0f;

	auto data = macro.exportData(false);

	f.write(reinterpret_cast<const char *>(data.data()), data.size());
	f.close();

	Common::showWithPriority(FLAlertLayer::create("Info", fmt::format("{} saved with {} inputs.", file, macro.inputs.size()), "Ok"));
}


std::optional<Macro> Macrobot::loadMacro(const std::string& file, bool inputs)
{
	std::ifstream f(Mod::get()->getSaveDir() / "macros" / (file + ".gdr"), std::ios::binary);

	if (!f)
	{
		f.close();
		return std::nullopt;
	}

	f.seekg(0, std::ios::end);
	size_t fileSize = f.tellg();
	f.seekg(0, std::ios::beg);

	std::vector<std::uint8_t> macroData(fileSize);

	f.read(reinterpret_cast<char *>(macroData.data()), fileSize);
	f.close();

	return Macro::importData(macroData, inputs);
}


bool Macrobot::load(const std::string& file)
{
	auto opt = loadMacro(file);

	if (!opt)
	{
		Common::showWithPriority(FLAlertLayer::create("Error", "Could not load macro!\n" + (Mod::get()->getSaveDir() / "macros" / (file + ".gdr")).string() + "!", "Ok"));
		return false;
	}

	macro = opt.value();

	if (macro.botInfo.name == "Macrobot" && macro.botInfo.version != "1.1")
	{
		macro = Macro();
		Common::showWithPriority(FLAlertLayer::create("Error", "This macro was recorded on an older macrobot version!", "Ok"));
		return false;
	}

	Common::showWithPriority(FLAlertLayer::create("Info", fmt::format("{} loaded with {} inputs.", file, macro.inputs.size()), "Ok"));
	PhysicsBypass::calculateTickrate();
	
	return true;
}

void Macrobot::remove(const std::string& file)
{
	std::filesystem::remove(Mod::get()->getSaveDir() / "macros" / (file + ".gdr"));
}

void Macrobot::drawMacroTable()
{
	static int macroIndex = 0;
	static std::string searchText = "";

	bool needReload = false;

	GUI::inputText("Search", &searchText);

	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {10, 10});
	if (!ImGui::BeginTable("MacroTable", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Resizable |
									ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBody))
	{
		ImGui::PopStyleVar();
		return;
	}

	ImGui::TableSetupColumn("Name");
	ImGui::TableSetupColumn("Author");
	ImGui::TableSetupColumn("FPS");
	ImGui::TableSetupColumn("Duration");
	ImGui::TableSetupColumn("Level");
	ImGui::TableSetupColumn("Actions");
	ImGui::TableHeadersRow();

	for(auto& pair : macroList)
	{
		Macro& macro = pair.second;

		if (!searchText.empty() && pair.first.find(searchText) == std::string::npos)
			continue;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%s", pair.first.c_str());
		ImGui::TableNextColumn();
		ImGui::Text("%s", macro.author.c_str());
		ImGui::TableNextColumn();
		ImGui::Text("%.2f", macro.framerate);
		ImGui::TableNextColumn();
		ImGui::Text("%.2f", macro.duration);
		ImGui::TableNextColumn();
		ImGui::Text("%s (%i)", macro.levelInfo.name.c_str(), macro.levelInfo.id);
		ImGui::TableNextColumn();
		if (GUI::button(("Load##macro" + pair.first).c_str()))
		{
			load(pair.first);
			ImGui::CloseCurrentPopup();
			GUI::toggle();
		}
		ImGui::SameLine();
		if (GUI::button(("Remove##macro" + pair.first).c_str()))
		{
			remove(pair.first);
			needReload = true;
		}
		ImGui::SameLine();
		if (GUI::button(("Play##macro" + pair.first).c_str()))
		{

			GameLevelManager::get()->downloadLevel(macro.levelInfo.id, true, 0);

			auto level = GameLevelManager::get()->getSavedLevel(macro.levelInfo.id);

			if (level && load(pair.first))
			{
				auto layer = PlayLayer::create(level, false, false);
				auto scene = CCScene::create();
				scene->addChild(layer);

				cocos2d::CCDirector::sharedDirector()->replaceScene(CCTransitionFade::create(0.5f, scene));

				Macrobot::playerMode = PLAYBACK;

				Common::calculateFramerate();
				PhysicsBypass::calculateTickrate();

				layer->stopAllActions();
				MBO(float, layer, 10960) = 0;//startgamedelayed
				layer->startGame();
				layer->resetLevelFromStart();

				ImGui::CloseCurrentPopup();
				GUI::toggle();
			}
		}
	}
	
	ImGui::EndTable();
	ImGui::PopStyleVar();

	if (needReload)
		getMacros();
}

bool Macrobot::clickBetweenFramesCheck()
{
	bool res = Loader::get()->getLoadedMod("syzzi.click_between_frames") != nullptr;

	if(res)
	{
		Common::showWithPriority(FLAlertLayer::create("Warning", "Disable the click between frames mod before using macrobot!", "Ok" ));
		Macrobot::playerMode = DISABLED;
		GUI::toggle();
	}

	return res;
}

#if defined(__ANDROID__)
static void stopAndroidPlayback()
{
	if (auto* pl = PlayLayer::get()) {
		botInput = true;
		for (const auto& [button, down] : playbackHeld1)
			if (down) pl->handleButton(false, button, true);
		for (const auto& [button, down] : playbackHeld2)
			if (down) pl->handleButton(false, button, false);
		botInput = false;
	}
	playbackHeld1.clear();
	playbackHeld2.clear();
	playerMode = DISABLED;
	actionIndex = 0;
}
#endif

void Macrobot::drawWindow()
{
#if defined(__ANDROID__)
	if (GUI::shouldRender())
	{
		if (playerMode == RECORDING)
		{
			ImGui::TextColored(ImVec4(1.f, 0.35f, 0.25f, 1.f), "Recording input...");
			if (GUI::button("Stop Recording##android-macro")) {
				for (const auto& [button, down] : recordingHeld1)
					if (down) recordAction(static_cast<PlayerButton>(button), currentFrame, false, true);
				for (const auto& [button, down] : recordingHeld2)
					if (down) recordAction(static_cast<PlayerButton>(button), currentFrame, false, false);
				recordingHeld1.clear();
				recordingHeld2.clear();
				playerMode = DISABLED;
				macro.duration = macro.framerate > 0.f ? currentFrame / macro.framerate : 0.f;
			}
		}
		else if (playerMode == PLAYBACK)
		{
			ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.f, 1.f), "Playing macro...");
			if (GUI::button("Stop Playback##android-macro"))
				stopAndroidPlayback();
		}
		else
		{
			if (GUI::button("Start Recording##android-macro"))
			{
				if (!PlayLayer::get())
					Common::showWithPriority(FLAlertLayer::create("Macro Bot", "Enter a level before recording.", "OK"));
				else
				{
					playerMode = RECORDING;
					currentFrame = 0;
					actionIndex = 0;
					macro.inputs.clear();
					playbackHeld1.clear();
					playbackHeld2.clear();
					PlayLayer::get()->resetLevel();
				}
			}
		}

		GUI::inputText("Name##android-macro", &macroName);
		GUI::inputText("Description##android-macro", &macroDescription);
		if (GUI::button("Save Macro##android-macro"))
		{
			if (playerMode != DISABLED)
				Common::showWithPriority(FLAlertLayer::create("Macro Bot", "Stop the current macro operation before saving.", "OK"));
			else {
				save(macroName);
				getMacros();
			}
		}
		if (GUI::button("Refresh Macros##android-macro"))
			getMacros();

		ImGui::Separator();
		ImGui::TextUnformatted("Saved macros");
		for (auto& pair : macroList)
		{
			ImGui::PushID(pair.first.c_str());
			ImGui::Text("%s", pair.first.c_str());
			ImGui::SameLine();
			if (GUI::button("Play"))
			{
				if (playerMode != DISABLED)
					Common::showWithPriority(FLAlertLayer::create("Macro Bot", "Stop the current macro operation first.", "OK"));
				else if (!PlayLayer::get())
					Common::showWithPriority(FLAlertLayer::create("Macro Bot", "Open a level before playback.", "OK"));
				else if (load(pair.first))
				{
					playerMode = PLAYBACK;
					actionIndex = 0;
					currentFrame = 0;
					playbackHeld1.clear();
					playbackHeld2.clear();
					PlayLayer::get()->resetLevel();
				}
			}
			ImGui::SameLine();
			if (GUI::button("Delete"))
			{
				remove(pair.first);
				getMacros();
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}
	}
	return;
#endif
	if (GUI::shouldRender())
	{
		if (ImGui::RadioButton("Disable", (int*)&Macrobot::playerMode, (int)DISABLED))
		{
			Common::calculateFramerate();
			PhysicsBypass::calculateTickrate();
		}
		if (ImGui::RadioButton("Record", (int*)&Macrobot::playerMode, (int)RECORDING) && !clickBetweenFramesCheck())
		{
			Common::calculateFramerate();
			PhysicsBypass::calculateTickrate();
			if (PlayLayer::get())
				PlayLayer::get()->resetLevel();
		}
		if (ImGui::RadioButton("Play", (int*)&Macrobot::playerMode, (int)PLAYBACK) && !clickBetweenFramesCheck())
		{
			Common::calculateFramerate();
			PhysicsBypass::calculateTickrate();
		}

		GUI::inputText("Name##macro", &macroName);
		GUI::inputText("Description##macro", &macroDescription);

		if (GUI::button("Save##macro"))
			save(macroName);
		GUI::sameLine();
		if (GUI::button("Load##macropopup"))
		{
			getMacros();
			ImGui::OpenPopup("Load Macro");
		}

		GUI::modalPopup("Load Macro", []{
			if (macroList.size() > 0)
				drawMacroTable();
			else
				ImGui::Text("There are no macros to load! Create some macros first.");
		});

		if (GUI::button("Editor"))
		{
			if(macro.inputs.size() <= 0)
			{
				Common::showWithPriority(FLAlertLayer::create("Error", "No macro loaded!", "Ok"));
				GUI::toggle();
			}
			else
				ImGui::OpenPopup("Macro Editor");
		}

		GUI::modalPopup("Macro Editor", []{
				MacroEditor::renderWindow();
		}, 0);

		int corrections = Settings::get<int>("macrobot/corrections");

		if (GUI::combo("Corrections", &corrections, correctionType, 2))
			Mod::get()->setSavedValue<int>("macrobot/corrections", corrections);

		GUI::tooltip("The type of correction the macro will use.\nNone: the bot will not correct on playback.\nEvery action: the bot will correct player's position, velocity and rotation every action.");

		GUI::checkbox("Click Sounds", "macrobot/clicks/enabled");
		GUI::tooltip("Play click sounds when the bot plays an action.");
		GUI::arrowButton("Clickpacks");
		Clickpacks::drawGUI();
	}

	GUI::checkbox("Frame Step", "macrobot/frame_step/enabled");
	GUI::tooltip("Makes the game only advance physics steps when the frame step button is pressed.");

	GUI::arrowButton("Frame Step Settings");
	GUI::modalPopup(
		"Frame Step Settings",
		[] {
			int key = Settings::get<int>("macrobot/frame_step/key", ImGuiKey_G);
			if (GUI::hotkey("Advance", &key))
				Mod::get()->setSavedValue<int>("macrobot/frame_step/key", key);

			GUI::inputInt("Advance Steps", "macrobot/frame_step/steps", 1);

			GUI::checkbox("Hold to Advance", "macrobot/frame_step/hold");
		},
		ImGuiWindowFlags_AlwaysAutoResize);

	GUI::marker("[INFO]", "Corrections are recommended to be safe, but the bot also works decently without.");
}

void Macrobot::getMacros()
{
#if defined(__ANDROID__)
	playerMode = DISABLED;
	Mod::get()->setSavedValue<bool>("macrobot/frame_step/enabled", false);
#endif
	macroList.clear();

	std::filesystem::path macroPath = Mod::get()->getSaveDir() / "macros";

	for (const auto& entry : std::filesystem::directory_iterator(macroPath))
	{
		if (entry.path().extension() == ".gdr")
		{
			std::string name = entry.path().stem().string();
			auto macroOpt = loadMacro(name, false);
			if (macroOpt)
				macroList.insert({name, macroOpt.value()});
		}
	}
}
