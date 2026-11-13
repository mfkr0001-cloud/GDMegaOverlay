#include "AudioChannelControl.h"
#include "../Common.h"

#include <Geode/Geode.hpp>
#include "Geode/fmod/fmod_dsp_effects.h"
#include <Geode/fmod/fmod.hpp>

#include <Geode/modify/PlayLayer.hpp>

#include <Geode/binding/FMODAudioEngine.hpp>

using namespace geode::prelude;

FMOD_RESULT AudioChannelControl::setVolumeHook(FMOD::Channel* channel, float volume)
{
	FMOD::Sound* sound;
	channel->getCurrentSound(&sound);
	float freq;
	sound->getDefaults(&freq, nullptr);
	channel->setFrequency(freq * speed);

	return channel->setVolume(volume);
}

void AudioChannelControl::set(float frequency)
{
	speed = frequency;

	auto* engine = FMODAudioEngine::sharedEngine();
	if (!engine || !engine->m_system)
		return;

	for(int i = 0; i < 2; i++)
	{
		FMOD::Channel* audioChannel = nullptr;

		if (engine->m_system->getChannel(126 + i, &audioChannel) != FMOD_OK || !audioChannel)
			continue;

		// Канал может существовать, но ничего не играть: sound будет nullptr.
		FMOD::Sound* sound = nullptr;
		if (audioChannel->getCurrentSound(&sound) != FMOD_OK || !sound)
			continue;

		float freq = 0.f;
		if (sound->getDefaults(&freq, nullptr) != FMOD_OK || freq <= 0.f)
			continue;

		audioChannel->setFrequency(freq * frequency);
	}
}

void AudioChannelControl::setPitch(float pitch)
{
	auto* engine = FMODAudioEngine::sharedEngine();
	if (!engine || !engine->m_system)
		return;

	auto system = engine->m_system;
	FMOD::ChannelGroup* group = nullptr;

	if (system->getMasterChannelGroup(&group) != FMOD_OK || !group)
		return;

	static FMOD::DSP* pitchShifter = nullptr;

	if (pitchShifter)
	{
		group->removeDSP(pitchShifter);
		pitchShifter->release();
		pitchShifter = nullptr;
	}

	if (pitch == 1.f)
		return;

	if (system->createDSPByType(FMOD_DSP_TYPE_PITCHSHIFT, &pitchShifter) != FMOD_OK || !pitchShifter)
	{
		pitchShifter = nullptr;
		return;
	}

	pitchShifter->setParameterFloat(FMOD_DSP_PITCHSHIFT_FFTSIZE, 4096);
	pitchShifter->setParameterFloat(FMOD_DSP_PITCHSHIFT_PITCH, pitch);
	group->addDSP(0, pitchShifter);
}

class $modify(PlayLayer)
{
	void resetLevel()
	{
		Common::onAudioSpeedChange();
		PlayLayer::resetLevel();
	}
};

#if defined(_WIN32)
$execute
{
	void* setVolumeAddress = reinterpret_cast<void*>(GetProcAddress(
		reinterpret_cast<HMODULE>(util::fmod_base), "?setVolume@ChannelControl@FMOD@@QAG?AW4FMOD_RESULT@@M@Z"));

	Mod::get()->hook(setVolumeAddress, &AudioChannelControl::setVolumeHook, "setVolumeHook", tulip::hook::TulipConvention::Stdcall);

	AudioChannelControl::speed = 1.f;
}
#endif
