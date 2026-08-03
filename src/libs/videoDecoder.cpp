#include "libs/videoDecoder.h"
#include "common/logging/log.h"
#include <algorithm>
#include <cstring>
#include <chrono>

namespace Kyty::Libs {

// Video Decoder Implementation
// Note: This is a framework implementation. Production would link against FFmpeg/libbink

VideoDecoder::VideoDecoder() {
    m_frameQueue.reserve(60); // 2 seconds at 30fps
}

VideoDecoder::~VideoDecoder() {
    Shutdown();
}

bool VideoDecoder::Initialize(const VideoDecoderParams& params) {
    if (m_initialized) {
        LOGF("[VideoDecoder] WARNING: " "Already initialized");
        return true;
    }

    // Validate parameters
    if (params.width <= 0 || params.width > VIDEO_DECODER_MAX_WIDTH ||
        params.height <= 0 || params.height > VIDEO_DECODER_MAX_HEIGHT) {
        LOGF("[VideoDecoder] ERROR: " "Invalid resolution: %dx%d", params.width, params.height);
        return false;
    }

    if (!IsCodecSupported(params.codec)) {
        LOGF("[VideoDecoder] ERROR: " "Unsupported codec: %d", static_cast<int>(params.codec));
        return false;
    }

    m_params = params;
    m_useHWAccel = params.enableHWAccel;

    // Initialize codec-specific decoder
    bool success = false;
    switch (params.codec) {
        case VideoCodec::H264_AVC:
            success = InitializeH264();
            break;
        case VideoCodec::H265_HEVC:
            success = InitializeH265();
            break;
        case VideoCodec::VP9:
            success = InitializeVP9();
            break;
        case VideoCodec::BINK:
            success = InitializeBink();
            break;
        default:
            LOGF("[VideoDecoder] ERROR: " "Unknown codec");
            return false;
    }

    if (!success) {
        LOGF("[VideoDecoder] ERROR: " "Failed to initialize codec %d", static_cast<int>(params.codec));
        return false;
    }

    m_initialized = true;
    ResetStats();

    LOGF("[VideoDecoder] INFO: " "Initialized: %s %dx%d @ %dfps, HW=%d",
             params.codec == VideoCodec::H264_AVC ? "H.264" :
             params.codec == VideoCodec::H265_HEVC ? "H.265" :
             params.codec == VideoCodec::VP9 ? "VP9" : "BINK",
             params.width, params.height,
             params.frameRateNum / params.frameRateDen,
             m_useHWAccel ? 1 : 0);

    return true;
}

void VideoDecoder::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Flush();
    
    // Clean up codec context (would free FFmpeg contexts in production)
    m_codecContext = nullptr;
    m_codecParser = nullptr;
    
    m_initialized = false;
    
    LOGF("[VideoDecoder] INFO: " "Shutdown complete");
}

bool VideoDecoder::InitializeH264() {
    // H.264/AVC initialization
    // In production: avcodec_find_decoder(AV_CODEC_ID_H264), avcodec_alloc_context3, etc.
    
    LOGF("[VideoDecoder] INFO: " "H.264 decoder initialized (stub)");
    return true;
}

bool VideoDecoder::InitializeH265() {
    // H.265/HEVC initialization
    // In production: avcodec_find_decoder(AV_CODEC_ID_HEVC), etc.
    
    LOGF("[VideoDecoder] INFO: " "H.265 decoder initialized (stub)");
    return true;
}

bool VideoDecoder::InitializeVP9() {
    // VP9 initialization
    // In production: avcodec_find_decoder(AV_CODEC_ID_VP9), etc.
    
    LOGF("[VideoDecoder] INFO: " "VP9 decoder initialized (stub)");
    return true;
}

bool VideoDecoder::InitializeBink() {
    // Bink Video initialization (used in many PS5 games for cutscenes)
    // In production: link against libbink or RAD Video Tools
    
    LOGF("[VideoDecoder] INFO: " "Bink decoder initialized (stub)");
    return true;
}

bool VideoDecoder::DecodePacket(const uint8_t* data, int32_t size, int64_t timestamp) {
    if (!m_initialized || !data || size <= 0) {
        return false;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    bool success = false;

    switch (m_params.codec) {
        case VideoCodec::H264_AVC:
            success = DecodeH264(data, size);
            break;
        case VideoCodec::H265_HEVC:
            success = DecodeH265(data, size);
            break;
        case VideoCodec::VP9:
            success = DecodeVP9(data, size);
            break;
        case VideoCodec::BINK:
            success = DecodeBink(data, size);
            break;
        default:
            return false;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double decodeTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    m_stats.totalFrames++;
    
    if (success) {
        m_stats.decodedFrames++;
        UpdateStats(decodeTime);
    } else {
        m_stats.decodeErrors++;
        LOGF("[VideoDecoder] WARNING: " "Decode error at timestamp %lld", timestamp);
    }

    return success;
}

bool VideoDecoder::DecodeH264(const uint8_t* data, int32_t size) {
    // H.264 decoding (stub - would use FFmpeg in production)
    // Parse NAL units, decode slices, output frames
    
    // For now, create a dummy frame to demonstrate the pipeline
    VideoFrame frame;
    frame.width = m_params.width;
    frame.height = m_params.height;
    frame.format = VideoPixelFormat::NV12;
    frame.isKeyFrame = true; // Simplified
    
    // Allocate Y plane
    frame.yPlane.resize(m_params.width * m_params.height);
    frame.strideY = m_params.width;
    
    // Allocate UV plane (interleaved, half resolution)
    frame.uPlane.resize((m_params.width / 2) * (m_params.height / 2) * 2);
    frame.strideUV = m_params.width;
    
    // Fill with test pattern (gray)
    std::fill(frame.yPlane.begin(), frame.yPlane.end(), 128);
    std::fill(frame.uPlane.begin(), frame.uPlane.end(), 128);
    
    m_frameQueue.push_back(frame);
    
    return true;
}

bool VideoDecoder::DecodeH265(const uint8_t* data, int32_t size) {
    // H.265 decoding (stub)
    // Similar to H.264 but with CTU, different prediction modes
    
    return DecodeH264(data, size); // Reuse H.264 stub for now
}

bool VideoDecoder::DecodeVP9(const uint8_t* data, int32_t size) {
    // VP9 decoding (stub)
    // Superblocks, intra/inter prediction, loop filters
    
    return DecodeH264(data, size); // Reuse H.264 stub for now
}

bool VideoDecoder::DecodeBink(const uint8_t* data, int32_t size) {
    // Bink Video decoding (stub)
    // Bink uses DCT, motion compensation, and custom compression
    
    // Bink often uses RGBA output
    VideoFrame frame;
    frame.width = m_params.width;
    frame.height = m_params.height;
    frame.format = VideoPixelFormat::RGBA32;
    frame.isKeyFrame = true;
    
    // Allocate RGBA data
    frame.rgbaData.resize(m_params.width * m_params.height * 4);
    
    // Fill with test pattern
    std::fill(frame.rgbaData.begin(), frame.rgbaData.end(), 255);
    
    m_frameQueue.push_back(frame);
    
    return true;
}

bool VideoDecoder::GetNextFrame(VideoFrame& frame) {
    if (m_frameQueue.empty()) {
        return false;
    }

    frame = std::move(m_frameQueue.front());
    m_frameQueue.erase(m_frameQueue.begin());
    
    return true;
}

bool VideoDecoder::HasPendingFrames() const {
    return !m_frameQueue.empty();
}

void VideoDecoder::SetFrameOutputCallback(FrameOutputCallback callback) {
    m_frameCallback = std::move(callback);
}

void VideoDecoder::OutputFrame(VideoFrame& frame) {
    if (m_frameCallback) {
        m_frameCallback(frame);
    }
}

bool VideoDecoder::Seek(int64_t timestamp) {
    if (!m_initialized) {
        return false;
    }

    // Flush current frames
    Flush();
    
    // In production, seek in the codec context
    LOGF("[VideoDecoder] INFO: " "Seek to timestamp %lld", timestamp);
    
    return true;
}

void VideoDecoder::SetPlaybackRate(float rate) {
    m_playbackRate = std::max(0.1f, std::min(10.0f, rate));
    LOGF("[VideoDecoder] INFO: " "Playback rate: %.2fx", m_playbackRate);
}

void VideoDecoder::Flush() {
    m_frameQueue.clear();
    LOGF("[VideoDecoder] INFO: " "Flushed frame queue");
}

void VideoDecoder::ResetStats() {
    m_stats = VideoDecoderStats();
}

void VideoDecoder::UpdateStats(double decodeTime) {
    // Exponential moving average
    const double alpha = 0.1;
    m_stats.averageDecodeTime = (1.0 - alpha) * m_stats.averageDecodeTime + alpha * decodeTime;
    
    // Calculate FPS
    if (m_stats.averageDecodeTime > 0) {
        m_stats.currentFPS = 1000.0 / m_stats.averageDecodeTime;
    }
}

VideoCodec VideoDecoder::DetectCodec(const uint8_t* data, int32_t size) {
    if (!data || size < 4) {
        return VideoCodec::UNKNOWN;
    }

    // Check for H.264 NAL unit start code
    if (size >= 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1) {
        uint8_t nalType = data[4] & 0x1F;
        if (nalType >= 1 && nalType <= 5) {
            return VideoCodec::H264_AVC;
        }
    }

    // Check for H.265 NAL unit
    if (size >= 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1) {
        uint8_t nalType = (data[4] >> 1) & 0x3F;
        if (nalType >= 1 && nalType <= 32) {
            return VideoCodec::H265_HEVC;
        }
    }

    // Check for Bink signature
    if (size >= 4 && data[0] == 'B' && data[1] == 'I' && data[2] == 'K' && data[3] == 'f') {
        return VideoCodec::BINK;
    }

    // Check for VP9 signature (in WebM container)
    if (size >= 4 && data[0] == 0x1A && data[1] == 0x45 && data[2] == 0xDF && data[3] == 0xA3) {
        return VideoCodec::VP9;
    }

    return VideoCodec::UNKNOWN;
}

VideoPixelFormat VideoDecoder::GetPixelFormat(int32_t codecId) {
    // Default formats for each codec
    switch (static_cast<VideoCodec>(codecId)) {
        case VideoCodec::H264_AVC:
        case VideoCodec::H265_HEVC:
            return VideoPixelFormat::NV12;
        case VideoCodec::VP9:
            return VideoPixelFormat::YUV420P;
        case VideoCodec::BINK:
            return VideoPixelFormat::RGBA32;
        default:
            return VideoPixelFormat::UNKNOWN;
    }
}

bool VideoDecoder::IsCodecSupported(VideoCodec codec) {
    // All listed codecs are supported (stubs)
    switch (codec) {
        case VideoCodec::H264_AVC:
        case VideoCodec::H265_HEVC:
        case VideoCodec::VP9:
        case VideoCodec::BINK:
            return true;
        default:
            return false;
    }
}

// VideoDecoderManager Implementation

VideoDecoderManager& VideoDecoderManager::Instance() {
    static VideoDecoderManager instance;
    return instance;
}

std::shared_ptr<VideoDecoder> VideoDecoderManager::CreateDecoder(const VideoDecoderParams& params) {
    auto decoder = std::make_shared<VideoDecoder>();
    
    if (!decoder->Initialize(params)) {
        LOGF("[VideoDecoderManager] ERROR: " "Failed to create decoder");
        return nullptr;
    }
    
    m_decoders.push_back(decoder);
    return decoder;
}

void VideoDecoderManager::DestroyDecoder(std::shared_ptr<VideoDecoder> decoder) {
    if (!decoder) {
        return;
    }
    
    decoder->Shutdown();
    
    auto it = std::find(m_decoders.begin(), m_decoders.end(), decoder);
    if (it != m_decoders.end()) {
        m_decoders.erase(it);
    }
}

int32_t VideoDecoderManager::GetActiveDecoderCount() const {
    return static_cast<int32_t>(m_decoders.size());
}

void VideoDecoderManager::ShutdownAll() {
    for (auto& decoder : m_decoders) {
        if (decoder) {
            decoder->Shutdown();
        }
    }
    m_decoders.clear();
}

VideoDecoderManager::~VideoDecoderManager() {
    ShutdownAll();
}

} // namespace Kyty::Libs
