#if !defined(__ANDROID__)

#include "AudioRecord.h"
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

#endif // !__ANDROID__
