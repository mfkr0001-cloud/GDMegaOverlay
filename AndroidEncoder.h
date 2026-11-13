#pragma once

#if defined(__ANDROID__)

#include <cstdint>
#include <string>
#include <vector>

// Minimal H.264 / MP4 video encoder built on the NDK media API (libmediandk).
// It is pure C++ (no Java/JNI) and is fed raw RGBA frames as returned by
// glReadPixels (bottom-up rows). Video only; no audio track.
class AndroidVideoEncoder
{
public:
	AndroidVideoEncoder() = default;
	~AndroidVideoEncoder();

	AndroidVideoEncoder(const AndroidVideoEncoder&) = delete;
	AndroidVideoEncoder& operator=(const AndroidVideoEncoder&) = delete;

	// Width/height are rounded down to a multiple of 16 (many hardware encoders
	// require aligned buffers). Use width()/height() for the real frame size.
	bool begin(const std::string& path, int width, int height, int fps, int bitrateBps);

	// rgbaBottomUp must hold width()*height()*4 bytes.
	bool addFrame(const uint8_t* rgbaBottomUp);

	// Flushes the encoder and finalizes the .mp4 file.
	bool finish();

	bool active() const { return m_codec != nullptr; }
	int width() const { return m_width; }
	int height() const { return m_height; }
	int64_t framesWritten() const { return m_frameIndex; }
	const std::string& error() const { return m_error; }

private:
	bool queueInput(const uint8_t* data, size_t size, uint32_t flags);
	bool drain(bool endOfStream);
	void release();

	struct AMediaCodec* m_codec = nullptr;
	struct AMediaMuxer* m_muxer = nullptr;
	int m_fd = -1;
	int m_track = -1;
	bool m_muxerStarted = false;

	int m_width = 0;
	int m_height = 0;
	int m_fps = 60;
	int64_t m_frameIndex = 0;

	std::vector<uint8_t> m_nv12;
	std::string m_error;
};

#endif // __ANDROID__
