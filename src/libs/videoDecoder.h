#pragma once

#include "libs.h"
#include "common/common.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace Kyty::Libs {

// Video Decoder - Supports H.264, H.265, VP9, and Bink Video
// Used for cutscene playback in PS5 games

constexpr int32_t VIDEO_DECODER_MAX_WIDTH = 3840; // 4K
constexpr int32_t VIDEO_DECODER_MAX_HEIGHT = 2160;
constexpr int32_t VIDEO_DECODER_MAX_STREAMS = 16;

enum class VideoCodec {
    H264_AVC,
    H265_HEVC,
    VP9,
    BINK,
    UNKNOWN
};

enum class VideoPixelFormat {
    NV12,
    P010,
    YUV420P,
    YUV422P,
    RGB24,
    RGBA32,
    UNKNOWN
};

struct VideoDecoderParams {
    VideoCodec codec = VideoCodec::H264_AVC;
    int32_t width = 1920;
    int32_t height = 1080;
    int32_t bitrate = 0;
    int32_t frameRateNum = 30;
    int32_t frameRateDen = 1;
    bool enableHWAccel = true;
    bool enableHDR = false;
};

struct VideoFrame {
    int32_t width = 0;
    int32_t height = 0;
    VideoPixelFormat format = VideoPixelFormat::NV12;
    int64_t timestamp = 0; // microseconds
    bool isKeyFrame = false;
    
    std::vector<uint8_t> yPlane;
    std::vector<uint8_t> uPlane;
    std::vector<uint8_t> vPlane;
    std::vector<uint8_t> rgbaData;
    
    int32_t strideY = 0;
    int32_t strideUV = 0;
};

struct VideoDecoderStats {
    int64_t totalFrames = 0;
    int64_t decodedFrames = 0;
    int64_t droppedFrames = 0;
    int64_t decodeErrors = 0;
    double averageDecodeTime = 0.0; // milliseconds
    double currentFPS = 0.0;
};

// Callback for frame output
using FrameOutputCallback = std::function<void(const VideoFrame& frame)>;

class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    // Initialization
    bool Initialize(const VideoDecoderParams& params);
    void Shutdown();

    // Decoding
    bool DecodePacket(const uint8_t* data, int32_t size, int64_t timestamp);
    bool DecodeFile(const std::string& filePath);
    void Flush();

    // Frame retrieval
    bool GetNextFrame(VideoFrame& frame);
    bool HasPendingFrames() const;

    // Callbacks
    void SetFrameOutputCallback(FrameOutputCallback callback);

    // Control
    bool Seek(int64_t timestamp);
    void SetPlaybackRate(float rate);
    float GetPlaybackRate() const { return m_playbackRate; }

    // State
    bool IsInitialized() const { return m_initialized; }
    VideoCodec GetCodec() const { return m_params.codec; }
    int32_t GetWidth() const { return m_params.width; }
    int32_t GetHeight() const { return m_params.height; }
    VideoDecoderStats GetStats() const { return m_stats; }
    void ResetStats();

    // Static helpers
    static VideoCodec DetectCodec(const uint8_t* data, int32_t size);
    static VideoPixelFormat GetPixelFormat(int32_t codecId);
    static bool IsCodecSupported(VideoCodec codec);

private:
    bool InitializeH264();
    bool InitializeH265();
    bool InitializeVP9();
    bool InitializeBink();

    bool DecodeH264(const uint8_t* data, int32_t size);
    bool DecodeH265(const uint8_t* data, int32_t size);
    bool DecodeVP9(const uint8_t* data, int32_t size);
    bool DecodeBink(const uint8_t* data, int32_t size);

    void OutputFrame(VideoFrame& frame);
    void UpdateStats(double decodeTime);

    bool m_initialized = false;
    VideoDecoderParams m_params;
    VideoDecoderStats m_stats;
    float m_playbackRate = 1.0f;

    std::vector<VideoFrame> m_frameQueue;
    FrameOutputCallback m_frameCallback;

    // Decoder state (simplified - would use FFmpeg/libbink in production)
    void* m_codecContext = nullptr;
    void* m_codecParser = nullptr;
    bool m_useHWAccel = false;
};

// Global decoder manager
class VideoDecoderManager {
public:
    static VideoDecoderManager& Instance();

    std::shared_ptr<VideoDecoder> CreateDecoder(const VideoDecoderParams& params);
    void DestroyDecoder(std::shared_ptr<VideoDecoder> decoder);
    int32_t GetActiveDecoderCount() const;
    void ShutdownAll();

private:
    VideoDecoderManager() = default;
    ~VideoDecoderManager();

    std::vector<std::shared_ptr<VideoDecoder>> m_decoders;
};

} // namespace Kyty::Libs
