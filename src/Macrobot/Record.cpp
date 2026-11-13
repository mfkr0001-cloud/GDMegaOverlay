
#include <filesystem>
#include "Record.h"
#include "../Common.h"
#include "../GUI/GUI.h"
#include "../Settings.hpp"

#include "AudioRecord.h"
#include "Macrobot.h"
#include "portable-file-dialogs.h"

#include <algorithm>
#include <cctype>
#include <ctime>

#include <Geode/cocos/platform/CCGL.h>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/ShaderLayer.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/Geode.hpp>

using namespace geode::prelude;
using namespace Record;

bool levelDone = false;
uintptr_t addr = 0;

#if !defined(__ANDROID__)
void glViewportHook(GLint a, GLint b, GLsizei c, GLsizei d)
{
    if (visiting && recorder.m_recording && inShader)
    {
        if (c != 2608 && d != 2608 && c != 1304 && d != 1304 && c != 652 && d != 652)
        {
            c = Settings::get<int>("recorder/resolution/x", 1920);
            d = Settings::get<int>("recorder/resolution/y", 1080);
        }
    }

    reinterpret_cast<void(__stdcall *)(GLint, GLint, GLsizei, GLsizei)>(addr)(a, b, c, d);
}

$execute
{
    void (__stdcall *glViewportFunc)(GLint, GLint, GLsizei, GLsizei) = glViewport;
    addr = reinterpret_cast<uintptr_t>(glViewportFunc);
    Mod::get()->hook(reinterpret_cast<void *>(addr), &glViewportHook, "glViewport", tulip::hook::TulipConvention::Stdcall);
}
#endif

class $modify(ShaderLayer)
{
    void visit()
    {
        inShader = true;
        ShaderLayer::visit();
        inShader = false;
    }
};

class $modify(PlayLayer)
{
    void onQuit()
    {
        if (recorder.m_recording)
            recorder.stop();

        PlayLayer::onQuit();
    }

    bool init(GJGameLevel* level, bool unk1, bool unk2)
    {
        bool res = PlayLayer::init(level, unk1, unk2);
        return res;
    }

    void levelComplete()
    {
        PlayLayer::levelComplete();
        levelDone = true;
    }

    void resetLevel()
    {
        levelDone = false;
        PlayLayer::resetLevel();
    }
};

class $modify(GJBaseGameLayer)
{
    void update(float dt)
    {
        if (recorder.m_recording)
            recorder.handle_recording(this, dt);

#if !defined(__ANDROID__)
        if (recorder.m_recording_audio)
            recorder.handle_recording_audio(this, dt);
#endif

        GJBaseGameLayer::update(dt);
    }
};

Recorder::Recorder()
    : m_width(1280), m_height(720), m_fps(60)
{}

void Recorder::start_audio()
{
#if defined(__ANDROID__)
    return;
#else
    std::string level_id = PlayLayer::get()->m_level->m_levelName.c_str() + ("_" + std::to_string(PlayLayer::get()->m_level->m_levelID.value()));
    std::filesystem::path path = renderPath;
    bool hasVideo = std::filesystem::exists(path);

    if (!hasVideo)
    {
        Common::showWithPriority(FLAlertLayer::create("Error", ("The render for the level has not been found!\n" + path.string()).c_str(), "Ok"));
        return;
    }

    levelDone = false;
    m_after_end_duration = Settings::get<float>("recorder/after_end", 3.4f);
    m_after_end_extra_time = 0.f;

    recorder.m_recording_audio = true;

    if (PlayLayer::get()->getChildByID("EndLevelLayer"))
        PlayLayer::get()->getChildByID("EndLevelLayer")->removeFromParent();

    PlayLayer::get()->stopAllActions();
    MBO(float, PlayLayer::get(), 10960) = 0;
    PlayLayer::get()->startGame();
    PlayLayer::get()->resetLevelFromStart();
    recorder.m_recording = false;
#endif
}

void Recorder::start()
{
#if defined(__ANDROID__)
    auto* playLayer = PlayLayer::get();
    if (!playLayer || m_recording)
        return;

    levelDone = false;

    // Keep the screen's aspect ratio so the level is not stretched.
    float aspect = 16.f / 9.f;
    if (auto* view = CCDirector::sharedDirector()->getOpenGLView())
    {
        const CCSize frame = view->getFrameSize();
        if (frame.width > 0.f && frame.height > 0.f)
            aspect = frame.width / frame.height;
    }

    int height = std::clamp(Settings::get<int>("recorder/android/height", 720), 144, 2160);
    int width = static_cast<int>(height * aspect + 0.5f);
    width = std::max(16, (width / 16) * 16);
    height = std::max(16, (height / 16) * 16);

    const int fps = std::clamp(Settings::get<int>("recorder/fps", 60), 15, 120);
    const int bitrateMbps = std::clamp(Settings::get<int>("recorder/android/bitrate", 8), 1, 100);

    std::string name = playLayer->m_level->m_levelName.c_str();
    for (auto& c : name)
        if (!std::isalnum(static_cast<unsigned char>(c)))
            c = '_';
    if (name.empty())
        name = "level";

    const auto outputPath = Mod::get()->getSaveDir() / "renders" /
        (name + "_" + std::to_string(playLayer->m_level->m_levelID.value()) + "_" + std::to_string(std::time(nullptr)) + ".mp4");
    renderPath = outputPath.string();

    // The encoder opens the file with open(2), which does not create directories.
    std::error_code dirError;
    std::filesystem::create_directories(outputPath.parent_path(), dirError);

    m_encoder = std::make_unique<AndroidVideoEncoder>();
    if (!m_encoder->begin(renderPath, width, height, fps, bitrateMbps * 1000000))
    {
        m_status = "Cannot start encoder: " + m_encoder->error();
        Common::showWithPriority(FLAlertLayer::create("Error", m_status.c_str(), "Ok"));
        m_encoder.reset();
        return;
    }

    m_width = m_encoder->width();
    m_height = m_encoder->height();
    m_fps = fps;

    m_current_frame.assign(static_cast<size_t>(m_width) * m_height * 4, 0);
    m_frame_has_data = false;
    m_finished_level = false;
    m_last_frame_t = m_extra_t = 0;
    m_frames_written = 0;
    tfx = 0;
    m_after_end_extra_time = 0.f;
    m_after_end_duration = Settings::get<float>("recorder/after_end", 3.4f);

    m_renderer.m_width = m_width;
    m_renderer.m_height = m_height;
    m_renderer.begin();

    m_recording = true;
    m_status = "Rendering " + std::to_string(m_width) + "x" + std::to_string(m_height) + " @ " + std::to_string(m_fps) + " fps...";

    if (playLayer->getChildByID("EndLevelLayer"))
        playLayer->getChildByID("EndLevelLayer")->removeFromParent();

    playLayer->stopAllActions();
    playLayer->startGame();
    playLayer->resetLevelFromStart();
#else
    levelDone = false;

    int resolution[2];
    resolution[0] = Settings::get<int>("recorder/resolution/x", 1920);
    resolution[1] = Settings::get<int>("recorder/resolution/y", 1080);

    int framerate = Settings::get<int>("recorder/fps", 60);

    m_width = resolution[0];
    m_height = resolution[1];
    m_fps = framerate;

    std::string codec = Settings::get<std::string>("recorder/codec", "");
    if (codec != "")
        m_codec = codec;

    std::string extraArgs =
        Settings::get<std::string>("recorder/extraArgs", "");

    if (extraArgs != "")
        m_extra_args = extraArgs;

    std::string extraArgsAfter =
        Settings::get<std::string>("recorder/extraArgsAfter", "-pix_fmt rgb0 -qp 16 -rc-lookahead 16 -preset slow");

    m_extra_args_after = "";
    if (extraArgsAfter != "")
        m_extra_args_after = extraArgsAfter;

    std::string extraArgsVideo =
        Settings::get<std::string>("recorder/extraArgsVideo", "colorspace=all=bt709:iall=bt470bg:fast=1");

    m_extra_args_video = "";
    if (extraArgsVideo != "")
        m_extra_args_video = extraArgsVideo;

    m_recording = true;
    m_frame_has_data = false;
    m_current_frame.resize(m_width * m_height * 3, 0);
    m_finished_level = false;
    m_last_frame_t = m_extra_t = 0;

    int bitrate = Settings::get<int>("recorder/bitrate", 30);
    m_bitrate = std::to_string(bitrate) + "M";

    float afterEnd = Settings::get<float>("recorder/after_end", 3.4f);

    m_after_end_extra_time = 0.f;
    m_after_end_duration = afterEnd;
    m_renderer.m_width = m_width;
    m_renderer.m_height = m_height;
    m_renderer.begin();
    tfx = 0;

    m_song_start_offset = PlayLayer::get()->m_levelSettings->m_songOffset;

    if (PlayLayer::get()->getChildByID("EndLevelLayer"))
        PlayLayer::get()->getChildByID("EndLevelLayer")->removeFromParent();

    PlayLayer::get()->stopAllActions();
    MBO(float, PlayLayer::get(), 10960) = 0;
    PlayLayer::get()->startGame();
    PlayLayer::get()->resetLevelFromStart();

    auto bg_volume = 1;
    auto sfx_volume = 1;
    auto song_offset = m_song_start_offset;

    std::thread([&, bg_volume, sfx_volume, song_offset]()
    {
        std::filesystem::path renderedVideo = renderPath;

        {
            std::stringstream stream;
            stream << '"' << m_ffmpeg_path << '"' << " -y -f rawvideo -pix_fmt rgb24 -s "
                   << m_width << "x" << m_height << " -r " << m_fps;

            if (!m_extra_args.empty())
                stream << " " << m_extra_args;

            stream << " -i - ";

            if (!m_codec.empty())
                stream << "-c:v " << m_codec << " ";

            if (!m_bitrate.empty())
                stream << "-b:v " << m_bitrate << " ";

            if (!m_extra_args_after.empty())
                stream << m_extra_args_after << " ";
            else
                stream << "-pix_fmt yuv420p ";

            stream << "-vf \"vflip";

            if (!m_extra_args_video.empty())
                stream << "," << m_extra_args_video;

            stream << "\" -an " << renderedVideo;

            auto process = subprocess::Popen(stream.str());

            while (m_recording || m_frame_has_data)
            {
                m_lock.lock();

                if (m_frame_has_data)
                {
                    const auto frame = m_current_frame;
                    m_frame_has_data = false;
                    m_lock.unlock();
                    process.m_stdin.write(frame.data(), frame.size());
                }
                else
                {
                    m_lock.unlock();
                }
            }

            try
            {
                process.close();
            }
            catch (const std::exception& e)
            {
                std::cout << e.what() << '\n';
            }
        }
    }).detach();
#endif
}

void Recorder::stop()
{
#if defined(__ANDROID__)
    if (!m_recording)
        return;

    m_recording = false;
    m_renderer.end();

    if (m_encoder)
    {
        const auto frames = m_encoder->framesWritten();
        if (m_encoder->finish())
            m_status = "Saved " + std::to_string(frames) + " frames: " + renderPath;
        else
            m_status = "Render failed: " + m_encoder->error();
        m_encoder.reset();
    }
#else
    m_renderer.end();
    m_recording = false;
#endif
}

#if !defined(__ANDROID__)
void MyRenderTexture::begin()
{
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &m_old_fbo);

    m_texture = new CCTexture2D;
    {
        auto data = malloc(m_width * m_height * 3);
        memset(data, 0, m_width * m_height * 3);
        m_texture->initWithData(data, kCCTexture2DPixelFormat_RGB888, m_width, m_height,
                                CCSize(static_cast<float>(m_width), static_cast<float>(m_height)));
        free(data);
    }

    glGetIntegerv(GL_RENDERBUFFER_BINDING_EXT, &m_old_rbo);

    glGenFramebuffersEXT(1, &m_fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);

    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_texture->getName(), 0);

    m_texture->setAliasTexParameters();

    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_old_rbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_old_fbo);
}

void MyRenderTexture::capture(std::mutex &lock, std::vector<u8> &data, volatile bool &lul)
{
    auto director = CCDirector::sharedDirector();
    glViewport(0, 0, m_width, m_height);

    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &m_old_fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);

    visiting = true;
    PlayLayer::get()->visit();
    visiting = false;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    lock.lock();
    lul = true;
    glReadPixels(0, 0, m_width, m_height, GL_RGB, GL_UNSIGNED_BYTE, data.data());
    lock.unlock();

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_old_fbo);
    director->setViewport();
}

void MyRenderTexture::end()
{
    m_texture->release();
}
#else
void MyRenderTexture::begin()
{
    GLint oldFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFbo);
    m_old_fbo = oldFbo;

    glGenTextures(1, &m_gl_texture);
    glBindTexture(GL_TEXTURE_2D, m_gl_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_gl_texture, 0);

    glGenRenderbuffers(1, &m_gl_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_gl_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, m_width, m_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_gl_depth);

    // The game clips some nodes with the stencil buffer, so try to provide one.
    glGenRenderbuffers(1, &m_gl_stencil);
    glBindRenderbuffer(GL_RENDERBUFFER, m_gl_stencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, m_width, m_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_gl_stencil);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        // Some GPUs reject separate depth + stencil attachments: drop the stencil.
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
        glDeleteRenderbuffers(1, &m_gl_stencil);
        m_gl_stencil = 0;
    }

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(m_old_fbo));
}

void MyRenderTexture::capture(std::mutex&, std::vector<u8>& data, volatile bool&)
{
    auto* director = CCDirector::sharedDirector();
    auto* playLayer = PlayLayer::get();
    if (!playLayer || m_fbo == 0)
        return;

    GLint previousFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    visiting = true;
    playLayer->visit();
    visiting = false;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFbo));
    director->setViewport();
}

void MyRenderTexture::end()
{
    if (m_fbo)
    {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_gl_texture)
    {
        glDeleteTextures(1, &m_gl_texture);
        m_gl_texture = 0;
    }
    if (m_gl_depth)
    {
        glDeleteRenderbuffers(1, &m_gl_depth);
        m_gl_depth = 0;
    }
    if (m_gl_stencil)
    {
        glDeleteRenderbuffers(1, &m_gl_stencil);
        m_gl_stencil = 0;
    }
}
#endif

void Recorder::capture_frame(int copies)
{
#if defined(__ANDROID__)
    if (!m_encoder)
        return;

    // Rendering and encoding are synchronous on Android: no worker thread needed.
    m_renderer.capture(m_lock, m_current_frame, m_frame_has_data);
    m_frame_has_data = false;

    for (int i = 0; i < std::max(1, copies); ++i)
    {
        if (!m_encoder->addFrame(m_current_frame.data()))
        {
            const std::string error = m_encoder->error();
            stop();
            m_status = "Render failed: " + (error.empty() ? std::string("encoder error") : error);
            return;
        }
        ++m_frames_written;
    }
#else
    (void)copies;
    while (m_frame_has_data)
    {
    }

    m_renderer.capture(m_lock, m_current_frame, m_frame_has_data);
#endif
}

void Recorder::stop_audio()
{
#if defined(__ANDROID__)
    return;
#else
    Common::showWithPriority(FLAlertLayer::create("Info", "Macro and sound rendered successfully!", "Ok"));
    AudioRecord::stop();
    m_recording_audio = false;

    std::filesystem::path video_path = renderPath;
    std::filesystem::path temp_path = video_path.parent_path() / "music.mp4";

    std::stringstream ss;

    ss << m_ffmpeg_path << " -y -i " << video_path << " -i fmodoutput.wav -c:v copy -map 0:v -map 1:a "
       << temp_path;

    auto process = subprocess::Popen(ss.str());

    try
    {
        process.close();
    }
    catch (const std::exception &e)
    {
        std::cout << e.what() << '\n';
    }

    std::filesystem::remove("fmodoutput.wav");
    std::filesystem::remove(video_path);
    std::filesystem::rename(temp_path, video_path);
#endif
}

#if !defined(__ANDROID__)
void Recorder::handle_recording_audio(GJBaseGameLayer *play_layer, float dt)
{
    if (!levelDone || m_after_end_extra_time < m_after_end_duration)
    {
        if (levelDone)
        {
            m_after_end_extra_time += dt;
            m_finished_level = true;
        }

        if (!AudioRecord::recording)
            AudioRecord::start();
    }
    else
    {
        if (AudioRecord::recording)
            stop_audio();
    }
}
#endif

#if !defined(__ANDROID__)
void Recorder::handle_recording(GJBaseGameLayer *play_layer, float dt)
{
    if (!levelDone || m_after_end_extra_time < m_after_end_duration)
    {
        if (levelDone)
        {
            m_after_end_extra_time += dt;
            m_finished_level = true;
        }

        tfx += dt;
        float timewarp = MBO(float, PlayLayer::get(), 724);

        auto frame_dt = 1. / static_cast<double>(m_fps) * timewarp;
        auto time = tfx + m_extra_t - m_last_frame_t;

        if (time >= frame_dt)
        {
            m_extra_t = time - frame_dt;
            m_last_frame_t = tfx;

            float time = (static_cast<float>(play_layer->m_gameState.m_unk1f8) / Common::getTPS()) * 1000.f;
            time += PlayLayer::get()->m_levelSettings->m_songOffset * 1000.f;

            FMOD::Channel* audioChannel;

            for (int i = 0; i < 2; i++)
            {
                FMODAudioEngine::sharedEngine()->m_system->getChannel(126 + i, &audioChannel);

                if (audioChannel)
                {
                    uint32_t channelTime = 0;
                    audioChannel->getPosition(&channelTime, FMOD_TIMEUNIT_MS);

                    if (channelTime <= 0)
                        continue;

                    if (channelTime - time > 0.15f)
                        audioChannel->setPosition(time, FMOD_TIMEUNIT_MS);
                }
            }

            capture_frame();
        }
    }
    else
    {
        stop();

        if (Settings::get<bool>("recorder/record_audio", false))
            start_audio();
        else
            Common::showWithPriority(FLAlertLayer::create("Info", "Macro rendered successfully!", "Ok"));
    }
}
#else
void Recorder::handle_recording(GJBaseGameLayer* play_layer, float dt)
{
    if (!levelDone || m_after_end_extra_time < m_after_end_duration)
    {
        if (levelDone)
        {
            m_after_end_extra_time += dt;
            m_finished_level = true;
        }

        tfx += dt;

        // Time scale of the level (speed portals / slow motion), so the video keeps constant game time per frame.
        float timewarp = play_layer->m_gameState.m_timeWarp;
        if (timewarp <= 0.f)
            timewarp = 1.f;

        const double frameDt = 1.0 / static_cast<double>(m_fps) * timewarp;
        const double time = tfx + m_extra_t - m_last_frame_t;

        if (time >= frameDt)
        {
            // If the device is slower than the video fps, repeat the frame so the video keeps its real speed.
            const int copies = std::clamp(static_cast<int>(time / frameDt), 1, 8);
            m_extra_t = time - frameDt * copies;
            m_last_frame_t = tfx;
            capture_frame(copies);
        }
    }
    else
    {
        stop();
        Common::showWithPriority(FLAlertLayer::create("Info", "Macro rendered successfully!\nVideo only (no sound yet).", "Ok"));
    }
}
#endif

void Record::drawWindow()
{
#if defined(__ANDROID__)
    if (recorder.m_recording)
    {
        ImGui::TextColored(ImVec4(1.f, 0.35f, 0.25f, 1.f), "Rendering... %u frames", recorder.m_frames_written);
        if (GUI::button("Stop Recording##android-render"))
            recorder.stop();
    }
    else
    {
        const bool disabled = !PlayLayer::get() || Macrobot::macro.inputs.empty() || Macrobot::playerMode != Macrobot::PLAYBACK;

        if (disabled)
            ImGui::BeginDisabled();

        if (GUI::button("Start Recording##android-render"))
            recorder.start();

        if (disabled)
        {
            GUI::tooltip("Load a macro and start playback inside a level first.");
            ImGui::EndDisabled();
        }
    }

    int height = Settings::get<int>("recorder/android/height", 720);
    GUI::inputInt("Video height", &height);
    if (ImGui::IsItemDeactivatedAfterEdit())
        Mod::get()->setSavedValue<int>("recorder/android/height", std::clamp(height, 144, 2160));
    GUI::tooltip("Width follows the screen aspect ratio. Sizes are rounded to a multiple of 16.");

    int framerate = Settings::get<int>("recorder/fps", 60);
    GUI::inputInt("Framerate", &framerate);
    if (ImGui::IsItemDeactivatedAfterEdit())
        Mod::get()->setSavedValue<int>("recorder/fps", std::clamp(framerate, 15, 120));

    int bitrate = Settings::get<int>("recorder/android/bitrate", 8);
    GUI::inputInt("Bitrate (Mbps)", &bitrate);
    if (ImGui::IsItemDeactivatedAfterEdit())
        Mod::get()->setSavedValue<int>("recorder/android/bitrate", std::clamp(bitrate, 1, 100));

    float afterEnd = Settings::get<float>("recorder/after_end", 3.4f);
    GUI::inputFloat("Show End For", &afterEnd);
    if (ImGui::IsItemDeactivatedAfterEdit())
        Mod::get()->setSavedValue<float>("recorder/after_end", afterEnd);

    if (!recorder.m_status.empty())
        ImGui::TextWrapped("%s", recorder.m_status.c_str());

    GUI::marker("[INFO]",
        "Android renderer: replays the loaded macro and encodes an H.264 .mp4 on the device "
        "(video only, no sound yet). Files are saved in the mod's renders folder.");
    return;
#else
    if (recorder.m_recording || recorder.m_recording_audio)
    {
        if (GUI::button("Stop Recording"))
        {
            if (recorder.m_recording)
                recorder.stop();
            else if (recorder.m_recording_audio)
                recorder.stop_audio();
        }
    }
    else
    {
        bool disabled = !PlayLayer::get() || Macrobot::macro.inputs.size() <= 0 || Macrobot::playerMode != 0;

        if (disabled)
            ImGui::BeginDisabled();

        if (GUI::button("Start Recording"))
        {
            if (!std::filesystem::exists("ffmpeg.exe"))
            {
                auto process = subprocess::Popen((Mod::get()->getResourcesDir() / "get_ffmpeg.exe").string());

                try
                {
                    process.close();
                }
                catch (const std::exception &e)
                {
                    std::cout << e.what() << '\n';
                }
            }

            if (std::filesystem::exists("ffmpeg.exe"))
            {
                renderPath = pfd::save_file("Save Render", (Mod::get()->getSaveDir() / "renders").string(), { "Video Files", "*.mp4" }).result();

                if (std::filesystem::path(renderPath).extension().string().empty())
                    renderPath.append(".mp4");

                if (!renderPath.empty())
                    Record::recorder.start();
            }
            else
            {
                Common::showWithPriority(FLAlertLayer::create("Error", "FFmpeg not found", "Ok"));
            }
        }

        if (disabled)
        {
            GUI::tooltip("You need to be playing a macro to record!");
            ImGui::EndDisabled();
        }
    }

    GUI::checkbox("Record audio", "recorder/record_audio");

    int resolution[2];
    resolution[0] = Settings::get<int>("recorder/resolution/x", 1920);
    resolution[1] = Settings::get<int>("recorder/resolution/y", 1080);

    GUI::inputInt2("Resolution", resolution, 256, 7680, 144, 4320);

    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        Mod::get()->setSavedValue<int>("recorder/resolution/x", resolution[0]);
        Mod::get()->setSavedValue<int>("recorder/resolution/y", resolution[1]);
    }

    int bitrate = Settings::get<int>("recorder/bitrate", 30);
    GUI::inputInt("Bitrate", &bitrate);

    if (ImGui::IsItemDeactivatedAfterEdit())
        Mod::get()->setSavedValue<int>("recorder/bitrate", bitrate);

    int framerate = Settings::get<int>("recorder/fps", 60);
    GUI::inputInt("Framerate", &framerate);

    if (ImGui::IsItemDeactivatedAfterEdit())
        Mod::get()->setSavedValue<int>("recorder/fps", framerate);

    float afterEnd = Settings::get<float>("recorder/after_end", 3.4f);
    GUI::inputFloat("Show End For", &afterEnd);

    if (ImGui::IsItemDeactivatedAfterEdit())
        Mod::get()->setSavedValue<float>("recorder/after_end", afterEnd);

    if (GUI::button("CPU"))
    {
        Mod::get()->setSavedValue<std::string>("recorder/codec", "");
        Mod::get()->setSavedValue<std::string>("recorder/extraArgs", "");
        Mod::get()->setSavedValue<std::string>("recorder/extraArgsAfter", "-pix_fmt rgb0 -qp 16 -rc-lookahead 16 -preset slow");
        Mod::get()->setSavedValue<std::string>("recorder/extraArgsVideo", "colorspace=all=bt709:iall=bt470bg:fast=1");
    }

    GUI::tooltip("Applies a preset for optimal rendering with the CPU");
    GUI::sameLine();

    if (GUI::button("NVIDIA"))
    {
        Mod::get()->setSavedValue<std::string>("recorder/codec", "h264_nvenc");
        Mod::get()->setSavedValue<std::string>("recorder/extraArgs", "-hwaccel cuda -hwaccel_output_format cuda");
        Mod::get()->setSavedValue<std::string>("recorder/extraArgsAfter", "-pix_fmt rgb0 -qp 16 -rc-lookahead 16 -preset slow");
        Mod::get()->setSavedValue<std::string>("recorder/extraArgsVideo", "colorspace=all=bt709:iall=bt470bg:fast=1");
    }

    GUI::tooltip("Applies a preset for optimal rendering with NVIDIA GPUs");
    GUI::sameLine();

    if (GUI::button("AMD"))
    {
        Mod::get()->setSavedValue<std::string>("recorder/codec", "h264_amf");
        Mod::get()->setSavedValue<std::string>("recorder/extraArgs", "");
        Mod::get()->setSavedValue<std::string>("recorder/extraArgsAfter", "-pix_fmt rgb0 -qp 16 -rc-lookahead 16 -preset slow");
        Mod::get()->setSavedValue<std::string>("recorder/extraArgsVideo", "colorspace=all=bt709:iall=bt470bg:fast=1");
    }

    GUI::tooltip("Applies a preset for optimal rendering with AMD GPUs");
    GUI::sameLine();

    if (GUI::button("Advanced Settings"))
        ImGui::OpenPopup("Advanced Settings##popup");

    GUI::tooltip("It is reccomended to use presets and not change these settings unless you know what you're doing!");

    GUI::modalPopup("Advanced Settings##popup", [] {
        std::string codec = Settings::get<std::string>("recorder/codec", "");

        if (GUI::inputText("Codec", &codec))
            Mod::get()->setSavedValue<std::string>("recorder/codec", codec);

        std::string extraArgs = Settings::get<std::string>("recorder/extraArgs", "");
        if (GUI::inputText("First Args", &extraArgs, 200))
            Mod::get()->setSavedValue<std::string>("recorder/extraArgs", extraArgs);

        std::string extraArgsAfter = Settings::get<std::string>("recorder/extraArgsAfter", "-pix_fmt rgb0 -qp 16 -rc-lookahead 16 -preset slow");
        if (GUI::inputText("Second Args", &extraArgsAfter, 200))
            Mod::get()->setSavedValue<std::string>("recorder/extraArgsAfter", extraArgsAfter);

        std::string extraArgsVideo = Settings::get<std::string>("recorder/extraArgsVideo", "colorspace=all=bt709:iall=bt470bg:fast=1");
        if (GUI::inputText("Video Args", &extraArgsVideo, 200))
            Mod::get()->setSavedValue<std::string>("recorder/extraArgsVideo", extraArgsVideo);
    });

    GUI::marker("[INFO]",
        "Press start recording to get a smooth recording of the level. "
        "To render music, clicks and sfx, press Start Music and wait for the level to finish again, then your "
        "rendered video will have music, clicks and sfx.");
#endif
}
