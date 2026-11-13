#pragma once

#include <cocos2d.h>
#include <thread>
#include <mutex>
#include <queue>
#include <vector>
#include <string>

#include <memory>

#if !defined(__ANDROID__)
#include <subprocess.hpp>
#else
#include "AndroidEncoder.h"
#endif

#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace cocos2d;

using u8 = uint8_t;

class MyRenderTexture
{
public:
    unsigned m_width, m_height;
    int m_old_fbo, m_old_rbo;
    unsigned m_fbo;
    CCTexture2D* m_texture;

    // Android: plain OpenGL ES render target (colour texture + depth/stencil).
    unsigned m_gl_texture = 0;
    unsigned m_gl_depth = 0;
    unsigned m_gl_stencil = 0;

    void begin();
    void end();
    void capture(std::mutex& lock, std::vector<u8>& data, volatile bool& lul);
};

class Recorder
{
public:
    Recorder();

    std::vector<u8> m_current_frame;
    volatile bool m_frame_has_data;
    std::mutex m_lock;

    MyRenderTexture m_renderer;

    unsigned m_width, m_height;
    unsigned m_fps;

    bool m_recording = false;

    double m_last_frame_t, m_extra_t;

    bool m_until_end = true;

    std::string m_codec = "";
    std::string m_bitrate = "30M";
    std::string m_extra_args = "";
    std::string m_extra_args_after = "";
    std::string m_extra_args_video = "";

    float m_after_end_duration;
    float m_after_end_extra_time;
    float m_song_start_offset;

    bool m_finished_level;
    bool m_include_audio = true;

    std::string m_ffmpeg_path = "ffmpeg";

    bool m_recording_audio = false;

    float tfx = 0;

    // Human readable state shown in the Recorder window.
    std::string m_status;
    unsigned m_frames_written = 0;

#if defined(__ANDROID__)
    std::unique_ptr<AndroidVideoEncoder> m_encoder;
#endif

    int sampleRate = 0;
    int numChannels = 0;
    int sampleFormat = 0;

    void start();
    void stop();
    void start_audio();
    void stop_audio();
    // `copies` > 1 repeats the captured frame (used when the device renders slower than the video fps).
    void capture_frame(int copies = 1);

    void handle_recording(GJBaseGameLayer*, float dt);
    void handle_recording_audio(GJBaseGameLayer*, float dt);
};

namespace Record
{
    inline Recorder recorder;
    inline bool visiting = false;
    inline bool inShader = false;
    inline std::string renderPath = "";

    void drawWindow();
}
