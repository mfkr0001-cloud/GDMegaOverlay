#pragma once

// Audio capture helper. The class is declared on every platform so that any
// translation unit can include this header safely. On desktop it switches FMOD
// to the WAV writer; on Android it is a no-op (Android recording is video only).
class AudioRecord
{
public:
    inline static bool recording = false;

    static void start();
    static void stop();
};
