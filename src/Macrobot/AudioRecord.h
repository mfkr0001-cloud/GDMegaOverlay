#pragma once

// Desktop-only audio capture helper. Android recording currently produces
// video only and does not compile or call this FMOD output-switching code.
class AudioRecord
{
public:
    inline static bool recording = false;

    static void start();
    static void stop();
};
