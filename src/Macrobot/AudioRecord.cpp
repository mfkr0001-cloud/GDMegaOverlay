#include "AndroidEncoder.h"

#if defined(__ANDROID__)

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaMuxer.h>

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>

namespace
{
	// MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar (NV12)
	constexpr int32_t kColorFormatYUV420SemiPlanar = 21;
	constexpr int64_t kTimeoutUs = 10000;

	inline uint8_t clamp8(int v)
	{
		return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
	}

	// RGBA (bottom-up, as read from OpenGL) -> NV12 (top-down), BT.709 limited range.
	void rgbaToNv12(const uint8_t* rgba, int w, int h, uint8_t* out)
	{
		uint8_t* yPlane = out;
		uint8_t* uvPlane = out + static_cast<size_t>(w) * h;

		for (int y = 0; y < h; ++y)
		{
			const uint8_t* row = rgba + static_cast<size_t>(h - 1 - y) * w * 4;
			uint8_t* yo = yPlane + static_cast<size_t>(y) * w;
			for (int x = 0; x < w; ++x)
			{
				const int r = row[x * 4 + 0];
				const int g = row[x * 4 + 1];
				const int b = row[x * 4 + 2];
				yo[x] = clamp8(((47 * r + 157 * g + 16 * b + 128) >> 8) + 16);
			}
		}

		for (int y = 0; y < h; y += 2)
		{
			const uint8_t* row0 = rgba + static_cast<size_t>(h - 1 - y) * w * 4;
			const uint8_t* row1 = rgba + static_cast<size_t>(h - 1 - (y + 1)) * w * 4;
			uint8_t* uvo = uvPlane + static_cast<size_t>(y / 2) * w;
			for (int x = 0; x < w; x += 2)
			{
				const uint8_t* p00 = row0 + x * 4;
				const uint8_t* p01 = row0 + (x + 1) * 4;
				const uint8_t* p10 = row1 + x * 4;
				const uint8_t* p11 = row1 + (x + 1) * 4;
				const int r = (p00[0] + p01[0] + p10[0] + p11[0] + 2) >> 2;
				const int g = (p00[1] + p01[1] + p10[1] + p11[1] + 2) >> 2;
				const int b = (p00[2] + p01[2] + p10[2] + p11[2] + 2) >> 2;
				uvo[x] = clamp8(((-26 * r - 87 * g + 112 * b + 128) >> 8) + 128);     // U (Cb)
				uvo[x + 1] = clamp8(((112 * r - 102 * g - 10 * b + 128) >> 8) + 128); // V (Cr)
			}
		}
	}
}

AndroidVideoEncoder::~AndroidVideoEncoder()
{
	release();
}

void AndroidVideoEncoder::release()
{
	if (m_muxer)
	{
		if (m_muxerStarted)
			AMediaMuxer_stop(m_muxer);
		AMediaMuxer_delete(m_muxer);
		m_muxer = nullptr;
	}
	m_muxerStarted = false;

	if (m_fd >= 0)
	{
		close(m_fd);
		m_fd = -1;
	}

	if (m_codec)
	{
		AMediaCodec_stop(m_codec);
		AMediaCodec_delete(m_codec);
		m_codec = nullptr;
	}

	m_track = -1;
}

bool AndroidVideoEncoder::begin(const std::string& path, int width, int height, int fps, int bitrateBps)
{
	release();
	m_error.clear();
	m_frameIndex = 0;

	m_width = std::max(16, (width / 16) * 16);
	m_height = std::max(16, (height / 16) * 16);
	m_fps = std::max(1, fps);
	m_nv12.assign(static_cast<size_t>(m_width) * m_height * 3 / 2, 0);

	m_codec = AMediaCodec_createEncoderByType("video/avc");
	if (!m_codec)
	{
		m_error = "No H.264 encoder available on this device";
		return false;
	}

	AMediaFormat* format = AMediaFormat_new();
	AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "video/avc");
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, m_width);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, m_height);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, kColorFormatYUV420SemiPlanar);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, bitrateBps);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, m_fps);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 1);

	media_status_t status = AMediaCodec_configure(m_codec, format, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
	AMediaFormat_delete(format);
	if (status != AMEDIA_OK)
	{
		m_error = "Encoder configure failed (" + std::to_string(static_cast<int>(status)) + ")";
		release();
		return false;
	}

	status = AMediaCodec_start(m_codec);
	if (status != AMEDIA_OK)
	{
		m_error = "Encoder start failed (" + std::to_string(static_cast<int>(status)) + ")";
		AMediaCodec_delete(m_codec);
		m_codec = nullptr;
		return false;
	}

	m_fd = open(path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);
	if (m_fd < 0)
	{
		m_error = "Cannot create output file: " + path;
		release();
		return false;
	}

	m_muxer = AMediaMuxer_new(m_fd, AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
	if (!m_muxer)
	{
		m_error = "Cannot create MP4 muxer";
		release();
		return false;
	}

	return true;
}

bool AndroidVideoEncoder::queueInput(const uint8_t* data, size_t size, uint32_t flags)
{
	ssize_t index = -1;
	for (int attempt = 0; attempt < 500; ++attempt)
	{
		index = AMediaCodec_dequeueInputBuffer(m_codec, kTimeoutUs);
		if (index >= 0)
			break;
		// Encoder input is full: pull finished output so it can make progress.
		if (!drain(false))
			return false;
	}

	if (index < 0)
	{
		m_error = "Encoder input timed out";
		return false;
	}

	size_t capacity = 0;
	uint8_t* buffer = AMediaCodec_getInputBuffer(m_codec, static_cast<size_t>(index), &capacity);
	if (!buffer || capacity < size)
	{
		m_error = "Encoder input buffer too small (needs " + std::to_string(size) + ", has " + std::to_string(capacity) + ")";
		return false;
	}

	if (size > 0)
		std::memcpy(buffer, data, size);

	const int64_t ptsUs = m_frameIndex * 1000000LL / m_fps;
	return AMediaCodec_queueInputBuffer(m_codec, static_cast<size_t>(index), 0, size, static_cast<uint64_t>(ptsUs), flags) == AMEDIA_OK;
}

bool AndroidVideoEncoder::drain(bool endOfStream)
{
	int idleSpins = 0;
	for (;;)
	{
		AMediaCodecBufferInfo info;
		const ssize_t index = AMediaCodec_dequeueOutputBuffer(m_codec, &info, endOfStream ? kTimeoutUs : 0);

		if (index == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
		{
			if (!endOfStream)
				return true;
			if (++idleSpins > 500)
			{
				m_error = "Encoder did not finish in time";
				return false;
			}
			continue;
		}

		if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
		{
			AMediaFormat* outFormat = AMediaCodec_getOutputFormat(m_codec);
			m_track = AMediaMuxer_addTrack(m_muxer, outFormat);
			AMediaFormat_delete(outFormat);
			if (m_track < 0 || AMediaMuxer_start(m_muxer) != AMEDIA_OK)
			{
				m_error = "Cannot start MP4 muxer";
				return false;
			}
			m_muxerStarted = true;
			continue;
		}

		if (index < 0)
			continue; // INFO_OUTPUT_BUFFERS_CHANGED or other informational code

		size_t outSize = 0;
		uint8_t* data = AMediaCodec_getOutputBuffer(m_codec, static_cast<size_t>(index), &outSize);

		if (info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG)
			info.size = 0; // SPS/PPS are already carried by the track format

		if (data && info.size > 0 && m_muxerStarted)
			AMediaMuxer_writeSampleData(m_muxer, static_cast<size_t>(m_track), data, &info);

		AMediaCodec_releaseOutputBuffer(m_codec, static_cast<size_t>(index), false);

		if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)
			return true;
	}
}

bool AndroidVideoEncoder::addFrame(const uint8_t* rgbaBottomUp)
{
	if (!m_codec || !rgbaBottomUp)
		return false;

	rgbaToNv12(rgbaBottomUp, m_width, m_height, m_nv12.data());

	if (!queueInput(m_nv12.data(), m_nv12.size(), 0))
		return false;

	++m_frameIndex;
	return drain(false);
}

bool AndroidVideoEncoder::finish()
{
	if (!m_codec)
		return false;

	bool ok = queueInput(nullptr, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
	if (ok)
		ok = drain(true);

	const bool wroteVideo = m_muxerStarted && m_frameIndex > 0;
	release();

	if (ok && !wroteVideo)
	{
		m_error = "No frames were encoded";
		return false;
	}
	return ok;
}

#endif // __ANDROID__
