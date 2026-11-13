#include "AudioRecord.h"

#if !defined(__ANDROID__)

#include <Geode/binding/FMODAudioEngine.hpp>

void AudioRecord::start()
{
    FMODAudioEngine::sharedEngine()->m_system->setOutput(FMOD_OUTPUTTYPE_WAVWRITER);
    recording = true;
}

void AudioRecord::stop()
{
    FMODAudioEngine::sharedEngine()->m_system->setOutput(FMOD_OUTPUTTYPE_AUTODETECT);
    recording = false;
}

#else // __ANDROID__

// No audio capture on Android: keep the symbols defined so the class links
// everywhere, but never claim that recording is active.
void AudioRecord::start()
{
    recording = false;
}

void AudioRecord::stop()
{
    recording = false;
}

#endif
