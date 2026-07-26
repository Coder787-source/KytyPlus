#include "common/abi.h"
#include "common/logging/log.h"
#include "libs/errno.h"
#include "libs/libs.h"
#include "loader/symbolDatabase.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_set>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

namespace Libs {

LIB_VERSION("Videodec2", 1, "Videodec2", 1, 1);

namespace VideoDec2 {

constexpr int32_t VIDEODEC2_ERROR_STRUCT_SIZE          = -2128805631; // 0x811d0101
constexpr int32_t VIDEODEC2_ERROR_ARGUMENT_POINTER     = -2128805630; // 0x811d0102
constexpr int32_t VIDEODEC2_ERROR_DECODER_INSTANCE     = -2128805629; // 0x811d0103
constexpr int32_t VIDEODEC2_ERROR_MEMORY_SIZE          = -2128805628; // 0x811d0104
constexpr int32_t VIDEODEC2_ERROR_MEMORY_POINTER       = -2128805627; // 0x811d0105
constexpr int32_t VIDEODEC2_ERROR_FRAME_BUFFER_SIZE    = -2128805626; // 0x811d0106
constexpr int32_t VIDEODEC2_ERROR_FRAME_BUFFER_POINTER = -2128805625; // 0x811d0107
constexpr int32_t VIDEODEC2_ERROR_CONFIG_INFO          = -2128805376; // 0x811d0200
constexpr int32_t VIDEODEC2_ERROR_COMPUTE_PIPE_ID      = -2128805375; // 0x811d0201
constexpr int32_t VIDEODEC2_ERROR_COMPUTE_QUEUE_ID     = -2128805374; // 0x811d0202
constexpr int32_t VIDEODEC2_ERROR_RESOURCE_TYPE        = -2128805373; // 0x811d0203
constexpr int32_t VIDEODEC2_ERROR_INPUT_QUEUE_DEPTH    = -2128805370; // 0x811d0206
constexpr int32_t VIDEODEC2_ERROR_DPB_FRAME_COUNT      = -2128805367; // 0x811d0209
constexpr int32_t VIDEODEC2_ERROR_FRAME_WIDTH_HEIGHT   = -2128805366; // 0x811d020a
// Not a confirmed Sony error code -- synthetic value (same 0x811d component prefix as the real
// codes above) used only to signal a host-side ffmpeg decode failure back to the caller.
constexpr int32_t VIDEODEC2_ERROR_API_FAIL = -2128804864; // 0x811d0400

constexpr uint32_t VIDEODEC2_RESOURCE_TYPE_COMPUTE = 1;
constexpr size_t   VIDEODEC2_MIN_MEMORY_SIZE       = 16ull * 1024ull * 1024ull;
constexpr uint32_t VIDEODEC2_FRAME_FORMAT_DEFAULT  = 0;
// Confirmed via the shadPS4 reference implementation of the same NIDs/struct layout
// (Libraries::Videodec2::VdecDecoder ctor asserts codecType == 1 for AVC/H264). No other
// codec_type value has a confirmed mapping, so only AVC is decoded for real; anything else
// is rejected explicitly rather than guessed at.
constexpr uint32_t VIDEODEC2_CODEC_TYPE_AVC = 1;

using Videodec2Decoder      = void*;
using Videodec2ComputeQueue = void*;

struct Videodec2DecoderConfigInfo {
	size_t                this_size;
	uint32_t              resource_type;
	uint32_t              codec_type;
	uint32_t              profile;
	uint32_t              max_level;
	int32_t               max_frame_width;
	int32_t               max_frame_height;
	int32_t               max_dpb_frame_count;
	uint32_t              decode_input_queue_depth;
	Videodec2ComputeQueue compute_queue;
	uint64_t              cpu_affinity_mask;
	int32_t               cpu_thread_priority;
	bool                  optimize_progressive_video;
	bool                  check_memory_type;
	uint8_t               reserved0;
	uint8_t               reserved1;
	void*                 extra_config_info;
};

struct Videodec2DecoderMemoryInfo {
	size_t   this_size;
	size_t   cpu_memory_size;
	void*    cpu_memory;
	size_t   gpu_memory_size;
	void*    gpu_memory;
	size_t   cpu_gpu_memory_size;
	void*    cpu_gpu_memory;
	size_t   max_frame_buffer_size;
	uint32_t frame_buffer_alignment;
	uint32_t reserved0;
};

struct Videodec2InputData {
	size_t   this_size;
	void*    au_data;
	size_t   au_size;
	uint64_t pts_data;
	uint64_t dts_data;
	uint64_t attached_data;
};

struct Videodec2OutputInfo {
	size_t   this_size;
	bool     is_valid;
	bool     is_error_frame;
	uint8_t  picture_count;
	bool     is_discarded_frame;
	uint32_t codec_type;
	uint32_t frame_width;
	uint32_t frame_pitch;
	uint32_t frame_height;
	void*    frame_buffer;
	size_t   frame_buffer_size;
	uint32_t frame_format;
	uint32_t frame_pitch_in_bytes;
};

struct Videodec2FrameBuffer {
	size_t this_size;
	void*  frame_buffer;
	size_t frame_buffer_size;
	bool   is_accepted;
};

struct Videodec2ComputeMemoryInfo {
	size_t this_size;
	size_t cpu_gpu_memory_size;
	void*  cpu_gpu_memory;
};

struct Videodec2ComputeConfigInfo {
	size_t   this_size;
	uint16_t compute_pipe_id;
	uint16_t compute_queue_id;
	bool     check_memory_type;
	uint8_t  reserved0;
	uint16_t reserved1;
};

struct DecoderState {
	uint64_t        magic;
	uint32_t        codec_type;
	AVCodecContext* codec_context = nullptr;
	SwsContext*     sws_context   = nullptr; // Lazily created only if a frame isn't already NV12.
};

static_assert(sizeof(Videodec2ComputeMemoryInfo) == 24);
static_assert(sizeof(Videodec2ComputeConfigInfo) == 16);
static_assert(sizeof(Videodec2DecoderConfigInfo) == 72);
static_assert(sizeof(Videodec2DecoderMemoryInfo) == 72);
static_assert(sizeof(Videodec2InputData) == 48);
static_assert(sizeof(Videodec2OutputInfo) == 56);
static_assert(sizeof(Videodec2FrameBuffer) == 32);

constexpr uint64_t DECODER_MAGIC = 0x4b59545956444543ull; // KYTYVDEC

static std::mutex                g_decoder_mutex;
static std::unordered_set<void*> g_decoders;

static bool IsOutputInfoSizeValid(size_t this_size) {
	return this_size == sizeof(Videodec2OutputInfo) ||
	       (this_size | 8u) == sizeof(Videodec2OutputInfo);
}

// Callers must keep g_decoder_mutex held for as long as the returned pointer is used. Returning
// the raw pointer after releasing the lock (as this used to do) let a concurrent DeleteDecoder
// free the DecoderState while Decode/Flush/Reset were still dereferencing it.
static DecoderState* GetDecoderLocked(Videodec2Decoder decoder) {
	return g_decoders.contains(decoder) ? static_cast<DecoderState*>(decoder) : nullptr;
}

static void FillNoPictureOutput(const Videodec2FrameBuffer* frame_buffer,
                                Videodec2OutputInfo* output_info, uint32_t codec_type) {
	output_info->is_valid           = false;
	output_info->is_error_frame     = false;
	output_info->picture_count      = 0;
	output_info->is_discarded_frame = false;
	output_info->codec_type         = codec_type;
	output_info->frame_width        = 0;
	output_info->frame_pitch        = 0;
	output_info->frame_height       = 0;
	output_info->frame_buffer      = frame_buffer != nullptr ? frame_buffer->frame_buffer : nullptr;
	output_info->frame_buffer_size = frame_buffer != nullptr ? frame_buffer->frame_buffer_size : 0;
	output_info->frame_format      = VIDEODEC2_FRAME_FORMAT_DEFAULT;
	output_info->frame_pitch_in_bytes = 0;
}

static uint32_t AlignUp(uint32_t value, uint32_t alignment) {
	return alignment == 0 ? value : ((value + alignment - 1u) / alignment) * alignment;
}

// Converts an arbitrary decoded frame to NV12 (matching the PS5 guest's expected surface format),
// reusing/creating `state`'s SwsContext as needed. Returns a new AVFrame the caller must free, or
// nullptr on failure. If `frame` is already NV12, no conversion happens and a reference is
// returned instead (still owned by the caller via av_frame_free).
static AVFrame* ConvertToNv12(DecoderState* state, const AVFrame* frame) {
	if (frame->format == AV_PIX_FMT_NV12) {
		AVFrame* ref = av_frame_alloc();
		if (ref == nullptr || av_frame_ref(ref, frame) < 0) {
			av_frame_free(&ref);
			return nullptr;
		}
		return ref;
	}

	AVFrame* nv12 = av_frame_alloc();
	if (nv12 == nullptr) {
		return nullptr;
	}
	nv12->format               = AV_PIX_FMT_NV12;
	nv12->width                = frame->width;
	nv12->height                = frame->height;
	nv12->sample_aspect_ratio   = frame->sample_aspect_ratio;
	nv12->crop_top              = frame->crop_top;
	nv12->crop_bottom           = frame->crop_bottom;
	nv12->crop_left             = frame->crop_left;
	nv12->crop_right            = frame->crop_right;
	nv12->pts                   = frame->pts;
	nv12->pkt_dts               = (frame->pkt_dts < 0 ? 0 : frame->pkt_dts);

	if (av_frame_get_buffer(nv12, 0) < 0) {
		av_frame_free(&nv12);
		return nullptr;
	}

	if (state->sws_context == nullptr) {
		state->sws_context =
		    sws_getContext(frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
		                   nv12->width, nv12->height, AV_PIX_FMT_NV12, SWS_FAST_BILINEAR, nullptr,
		                   nullptr, nullptr);
	}
	if (state->sws_context == nullptr ||
	    sws_scale(state->sws_context, frame->data, frame->linesize, 0, frame->height, nv12->data,
	             nv12->linesize) < 0) {
		av_frame_free(&nv12);
		return nullptr;
	}

	return nv12;
}

// Copies an NV12 frame into the guest-provided frame buffer using the PS5 AVC decode surface
// layout (luma plane, then chroma plane, each row-padded to `pitch`). Unlike a naive copy, this
// validates the destination is actually large enough for the full padded surface before writing
// anything -- a title reporting a frame_buffer_size smaller than pitch*height*3/2 gets a clean
// error instead of a heap overflow.
static bool CopyNv12ToFrameBuffer(uint8_t* dst, size_t dst_capacity, const AVFrame& src,
                                  uint32_t pitch, uint32_t height) {
	const auto required = static_cast<size_t>(pitch) * height * 3u / 2u;
	if (dst == nullptr || required > dst_capacity) {
		return false;
	}

	auto* luma_dst   = dst;
	auto* chroma_dst = dst + static_cast<size_t>(pitch) * height;

	const auto src_width  = static_cast<uint32_t>(std::max(src.width, 0));
	const auto src_height = static_cast<uint32_t>(std::max(src.height, 0));
	const auto copy_width  = std::min(src_width, pitch);
	const auto copy_height = std::min(src_height, height);

	for (uint32_t y = 0; y < copy_height; y++) {
		std::memcpy(luma_dst + static_cast<size_t>(y) * pitch,
		           src.data[0] + static_cast<size_t>(y) * src.linesize[0], copy_width);
	}
	for (uint32_t y = 0; y < copy_height / 2u; y++) {
		std::memcpy(chroma_dst + static_cast<size_t>(y) * pitch,
		           src.data[1] + static_cast<size_t>(y) * src.linesize[1], copy_width);
	}

	// Extend the last valid row downward/rightward into any alignment padding, matching the
	// real hardware decoder's border-extend behavior for cropped dimensions (avoids leaving
	// uninitialized padding that some titles sample from at surface edges).
	if (copy_height > 0 && copy_height < height) {
		for (uint32_t y = copy_height; y < height; y++) {
			std::memcpy(luma_dst + static_cast<size_t>(y) * pitch,
			           luma_dst + static_cast<size_t>(copy_height - 1) * pitch, pitch);
		}
		for (uint32_t y = copy_height / 2u; y < height / 2u; y++) {
			std::memcpy(chroma_dst + static_cast<size_t>(y) * pitch,
			           chroma_dst + static_cast<size_t>(std::max<uint32_t>(copy_height / 2u, 1u) - 1) *
			                           pitch,
			           pitch);
		}
	}

	return true;
}

// Shared AVC decode path for Decode()/Flush(). `packet` is nullptr for a flush (drain) call.
// Returns OK with output_info->is_valid left false if the decoder legitimately has no frame
// ready yet (EAGAIN/EOF) -- that is not an error, just "no picture this call".
static int32_t DecodeCommon(DecoderState* state, AVPacket* packet,
                            Videodec2FrameBuffer* frame_buffer, Videodec2OutputInfo* output_info) {
	int send_result = avcodec_send_packet(state->codec_context, packet);
	// AVERROR_EOF from a second flush call after the decoder is already drained is not an error.
	if (send_result < 0 && send_result != AVERROR_EOF) {
		FillNoPictureOutput(frame_buffer, output_info, state->codec_type);
		output_info->is_error_frame = true;
		return VIDEODEC2_ERROR_API_FAIL;
	}

	AVFrame* frame = av_frame_alloc();
	if (frame == nullptr) {
		FillNoPictureOutput(frame_buffer, output_info, state->codec_type);
		return VIDEODEC2_ERROR_API_FAIL;
	}

	const int receive_result = avcodec_receive_frame(state->codec_context, frame);
	if (receive_result == AVERROR(EAGAIN) || receive_result == AVERROR_EOF) {
		av_frame_free(&frame);
		frame_buffer->is_accepted = false;
		FillNoPictureOutput(frame_buffer, output_info, state->codec_type);
		return OK;
	}
	if (receive_result < 0) {
		av_frame_free(&frame);
		FillNoPictureOutput(frame_buffer, output_info, state->codec_type);
		output_info->is_error_frame = true;
		return VIDEODEC2_ERROR_API_FAIL;
	}

	AVFrame* nv12 = ConvertToNv12(state, frame);
	av_frame_free(&frame);
	if (nv12 == nullptr) {
		FillNoPictureOutput(frame_buffer, output_info, state->codec_type);
		output_info->is_error_frame = true;
		return VIDEODEC2_ERROR_API_FAIL;
	}

	const auto width  = AlignUp(static_cast<uint32_t>(std::max(nv12->width, 0)), 16u);
	const auto pitch  = AlignUp(static_cast<uint32_t>(std::max(nv12->width, 0)), 64u);
	const auto height = AlignUp(static_cast<uint32_t>(std::max(nv12->height, 0)), 16u);

	if (!CopyNv12ToFrameBuffer(static_cast<uint8_t*>(frame_buffer->frame_buffer),
	                          frame_buffer->frame_buffer_size, *nv12, pitch, height)) {
		av_frame_free(&nv12);
		frame_buffer->is_accepted = false;
		FillNoPictureOutput(frame_buffer, output_info, state->codec_type);
		output_info->is_error_frame = true;
		return VIDEODEC2_ERROR_FRAME_BUFFER_SIZE;
	}

	frame_buffer->is_accepted = true;

	output_info->is_valid           = true;
	output_info->is_error_frame     = false;
	output_info->picture_count      = 1;
	output_info->is_discarded_frame = false;
	output_info->codec_type         = state->codec_type;
	output_info->frame_width        = width;
	output_info->frame_pitch        = pitch;
	output_info->frame_height       = height;
	output_info->frame_buffer       = frame_buffer->frame_buffer;
	output_info->frame_buffer_size  = static_cast<size_t>(pitch) * height * 3u / 2u;
	output_info->frame_format       = VIDEODEC2_FRAME_FORMAT_DEFAULT;
	if (output_info->this_size == sizeof(Videodec2OutputInfo)) {
		output_info->frame_pitch_in_bytes = pitch;
	}

	av_frame_free(&nv12);
	return OK;
}

static int32_t ValidateDecoderConfig(const Videodec2DecoderConfigInfo* config) {
	if (config->resource_type != VIDEODEC2_RESOURCE_TYPE_COMPUTE) {
		return VIDEODEC2_ERROR_RESOURCE_TYPE;
	}

	if (config->reserved0 != 0 || config->reserved1 != 0) {
		return VIDEODEC2_ERROR_CONFIG_INFO;
	}

	if (config->decode_input_queue_depth == 0) {
		return VIDEODEC2_ERROR_INPUT_QUEUE_DEPTH;
	}

	if (config->max_dpb_frame_count < -1 || config->max_dpb_frame_count == 0) {
		return VIDEODEC2_ERROR_DPB_FRAME_COUNT;
	}

	if (config->max_frame_width < -1 || config->max_frame_height < -1 ||
	    config->max_frame_width == 0 || config->max_frame_height == 0) {
		return VIDEODEC2_ERROR_FRAME_WIDTH_HEIGHT;
	}

	if (config->compute_queue == nullptr) {
		return VIDEODEC2_ERROR_CONFIG_INFO;
	}

	return OK;
}

static int32_t KYTY_SYSV_ABI
QueryComputeMemoryInfo(Videodec2ComputeMemoryInfo* compute_memory_info) {
	PRINT_NAME();

	if (compute_memory_info == nullptr) {
		return VIDEODEC2_ERROR_ARGUMENT_POINTER;
	}

	if (compute_memory_info->this_size != sizeof(Videodec2ComputeMemoryInfo)) {
		return VIDEODEC2_ERROR_STRUCT_SIZE;
	}

	compute_memory_info->cpu_gpu_memory_size = VIDEODEC2_MIN_MEMORY_SIZE;
	compute_memory_info->cpu_gpu_memory      = nullptr;

	return OK;
}

static int32_t KYTY_SYSV_ABI AllocateComputeQueue(
    const Videodec2ComputeConfigInfo* compute_config_info,
    const Videodec2ComputeMemoryInfo* compute_memory_info, Videodec2ComputeQueue* compute_queue) {
	PRINT_NAME();

	if (compute_config_info == nullptr || compute_memory_info == nullptr ||
	    compute_queue == nullptr) {
		return VIDEODEC2_ERROR_ARGUMENT_POINTER;
	}

	if (compute_config_info->this_size != sizeof(Videodec2ComputeConfigInfo) ||
	    compute_memory_info->this_size != sizeof(Videodec2ComputeMemoryInfo)) {
		return VIDEODEC2_ERROR_STRUCT_SIZE;
	}

	if (compute_config_info->reserved0 != 0 || compute_config_info->reserved1 != 0) {
		return VIDEODEC2_ERROR_CONFIG_INFO;
	}

	if (compute_config_info->compute_pipe_id > 4) {
		return VIDEODEC2_ERROR_COMPUTE_PIPE_ID;
	}

	if (compute_config_info->compute_queue_id > 7) {
		return VIDEODEC2_ERROR_COMPUTE_QUEUE_ID;
	}

	if (compute_memory_info->cpu_gpu_memory_size < VIDEODEC2_MIN_MEMORY_SIZE) {
		return VIDEODEC2_ERROR_MEMORY_SIZE;
	}

	if (compute_memory_info->cpu_gpu_memory == nullptr) {
		return VIDEODEC2_ERROR_MEMORY_POINTER;
	}

	*compute_queue = compute_memory_info->cpu_gpu_memory;

	return OK;
}

static int32_t KYTY_SYSV_ABI ReleaseComputeQueue(Videodec2ComputeQueue compute_queue) {
	PRINT_NAME();

	return compute_queue != nullptr ? OK : VIDEODEC2_ERROR_COMPUTE_QUEUE_ID;
}

static int32_t KYTY_SYSV_ABI QueryDecoderMemoryInfo(const Videodec2DecoderConfigInfo* config,
                                                    Videodec2DecoderMemoryInfo*       memory_info) {
	PRINT_NAME();

	if (config == nullptr || memory_info == nullptr) {
		return VIDEODEC2_ERROR_ARGUMENT_POINTER;
	}

	if (config->this_size != sizeof(Videodec2DecoderConfigInfo) ||
	    memory_info->this_size != sizeof(Videodec2DecoderMemoryInfo)) {
		return VIDEODEC2_ERROR_STRUCT_SIZE;
	}

	const auto validation_result = ValidateDecoderConfig(config);
	if (validation_result != OK) {
		return validation_result;
	}

	memory_info->cpu_memory_size        = VIDEODEC2_MIN_MEMORY_SIZE;
	memory_info->cpu_memory             = nullptr;
	memory_info->gpu_memory_size        = VIDEODEC2_MIN_MEMORY_SIZE;
	memory_info->gpu_memory             = nullptr;
	memory_info->cpu_gpu_memory_size    = VIDEODEC2_MIN_MEMORY_SIZE;
	memory_info->cpu_gpu_memory         = nullptr;
	memory_info->max_frame_buffer_size  = VIDEODEC2_MIN_MEMORY_SIZE;
	memory_info->frame_buffer_alignment = 0x100;
	memory_info->reserved0              = 0;

	return OK;
}

static int32_t KYTY_SYSV_ABI CreateDecoder(const Videodec2DecoderConfigInfo* config,
                                           const Videodec2DecoderMemoryInfo* memory_info,
                                           Videodec2Decoder*                 decoder) {
	PRINT_NAME();

	if (config == nullptr || memory_info == nullptr || decoder == nullptr) {
		return VIDEODEC2_ERROR_ARGUMENT_POINTER;
	}

	if (config->this_size != sizeof(Videodec2DecoderConfigInfo) ||
	    memory_info->this_size != sizeof(Videodec2DecoderMemoryInfo)) {
		return VIDEODEC2_ERROR_STRUCT_SIZE;
	}

	const auto validation_result = ValidateDecoderConfig(config);
	if (validation_result != OK) {
		return validation_result;
	}

	if (memory_info->cpu_memory_size < VIDEODEC2_MIN_MEMORY_SIZE ||
	    memory_info->gpu_memory_size < VIDEODEC2_MIN_MEMORY_SIZE ||
	    memory_info->cpu_gpu_memory_size < VIDEODEC2_MIN_MEMORY_SIZE ||
	    memory_info->max_frame_buffer_size < VIDEODEC2_MIN_MEMORY_SIZE) {
		return VIDEODEC2_ERROR_MEMORY_SIZE;
	}

	if (memory_info->cpu_memory == nullptr || memory_info->gpu_memory == nullptr ||
	    memory_info->cpu_gpu_memory == nullptr) {
		return VIDEODEC2_ERROR_MEMORY_POINTER;
	}

	// Only AVC has a confirmed codec_type mapping (see VIDEODEC2_CODEC_TYPE_AVC above). Reject
	// anything else explicitly instead of silently no-op'ing or guessing an ffmpeg codec id.
	if (config->codec_type != VIDEODEC2_CODEC_TYPE_AVC) {
		LOGF_COLOR(Log::Color::Yellow,
		           "Videodec2: unsupported codec_type=%" PRIu32 " (only AVC/H264 is implemented)\n",
		           config->codec_type);
		return VIDEODEC2_ERROR_CONFIG_INFO;
	}

	const auto* av_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (av_codec == nullptr) {
		return VIDEODEC2_ERROR_API_FAIL;
	}

	auto* codec_context = avcodec_alloc_context3(av_codec);
	if (codec_context == nullptr) {
		return VIDEODEC2_ERROR_API_FAIL;
	}
	codec_context->width  = config->max_frame_width;
	codec_context->height = config->max_frame_height;

	if (avcodec_open2(codec_context, av_codec, nullptr) < 0) {
		avcodec_free_context(&codec_context);
		return VIDEODEC2_ERROR_API_FAIL;
	}

	auto* state          = new DecoderState {};
	state->magic         = DECODER_MAGIC;
	state->codec_type    = config->codec_type;
	state->codec_context = codec_context;

	{
		std::scoped_lock lock(g_decoder_mutex);
		g_decoders.insert(state);
	}

	*decoder = state;

	return OK;
}

static int32_t KYTY_SYSV_ABI DeleteDecoder(Videodec2Decoder decoder) {
	PRINT_NAME();

	DecoderState* state = nullptr;
	{
		std::scoped_lock lock(g_decoder_mutex);
		auto             it = g_decoders.find(decoder);
		if (it == g_decoders.end()) {
			return VIDEODEC2_ERROR_DECODER_INSTANCE;
		}
		state = static_cast<DecoderState*>(*it);
		g_decoders.erase(it);
	}

	if (state->codec_context != nullptr) {
		avcodec_free_context(&state->codec_context);
	}
	if (state->sws_context != nullptr) {
		sws_freeContext(state->sws_context);
	}
	delete state;

	return OK;
}

static int32_t KYTY_SYSV_ABI Decode(Videodec2Decoder decoder, const Videodec2InputData* input_data,
                                    Videodec2FrameBuffer* frame_buffer,
                                    Videodec2OutputInfo*  output_info) {
	PRINT_NAME();

	std::scoped_lock lock(g_decoder_mutex);

	auto* state = GetDecoderLocked(decoder);
	if (state == nullptr || state->magic != DECODER_MAGIC) {
		return VIDEODEC2_ERROR_DECODER_INSTANCE;
	}

	if (input_data == nullptr || frame_buffer == nullptr || output_info == nullptr) {
		return VIDEODEC2_ERROR_ARGUMENT_POINTER;
	}

	if (input_data->this_size != sizeof(Videodec2InputData) ||
	    frame_buffer->this_size != sizeof(Videodec2FrameBuffer) ||
	    !IsOutputInfoSizeValid(output_info->this_size)) {
		return VIDEODEC2_ERROR_STRUCT_SIZE;
	}

	if (input_data->au_size != 0 && input_data->au_data == nullptr) {
		return VIDEODEC2_ERROR_ARGUMENT_POINTER;
	}

	if (frame_buffer->frame_buffer_size == 0) {
		return VIDEODEC2_ERROR_FRAME_BUFFER_SIZE;
	}

	if (frame_buffer->frame_buffer == nullptr) {
		return VIDEODEC2_ERROR_FRAME_BUFFER_POINTER;
	}

	frame_buffer->is_accepted = false;

	if (input_data->au_size == 0) {
		// Nothing to feed the decoder this call; report "no picture yet", not an error.
		FillNoPictureOutput(frame_buffer, output_info, state->codec_type);
		return OK;
	}

	AVPacket* packet = av_packet_alloc();
	if (packet == nullptr) {
		FillNoPictureOutput(frame_buffer, output_info, state->codec_type);
		return VIDEODEC2_ERROR_API_FAIL;
	}
	packet->data = static_cast<uint8_t*>(input_data->au_data);
	packet->size = static_cast<int>(input_data->au_size);
	packet->pts  = static_cast<int64_t>(input_data->pts_data);
	packet->dts  = static_cast<int64_t>(input_data->dts_data);

	const auto result = DecodeCommon(state, packet, frame_buffer, output_info);

	// packet->data points at guest memory owned by the caller, not something ffmpeg allocated;
	// av_packet_free must not try to free it. Clear it first so the free only releases the
	// AVPacket struct itself.
	packet->data = nullptr;
	packet->size = 0;
	av_packet_free(&packet);

	return result;
}

static int32_t KYTY_SYSV_ABI Flush(Videodec2Decoder decoder, Videodec2FrameBuffer* frame_buffer,
                                   Videodec2OutputInfo* output_info) {
	PRINT_NAME();

	std::scoped_lock lock(g_decoder_mutex);

	auto* state = GetDecoderLocked(decoder);
	if (state == nullptr || state->magic != DECODER_MAGIC) {
		return VIDEODEC2_ERROR_DECODER_INSTANCE;
	}

	if (frame_buffer == nullptr || output_info == nullptr) {
		return VIDEODEC2_ERROR_ARGUMENT_POINTER;
	}

	if (frame_buffer->this_size != sizeof(Videodec2FrameBuffer) ||
	    !IsOutputInfoSizeValid(output_info->this_size)) {
		return VIDEODEC2_ERROR_STRUCT_SIZE;
	}

	frame_buffer->is_accepted = false;

	// A flush call sends a null packet to drain any buffered frames out of the decoder.
	return DecodeCommon(state, nullptr, frame_buffer, output_info);
}

static int32_t KYTY_SYSV_ABI Reset(Videodec2Decoder decoder) {
	PRINT_NAME();

	std::scoped_lock lock(g_decoder_mutex);

	auto* state = GetDecoderLocked(decoder);
	if (state == nullptr || state->magic != DECODER_MAGIC) {
		return VIDEODEC2_ERROR_DECODER_INSTANCE;
	}

	if (state->codec_context != nullptr) {
		avcodec_flush_buffers(state->codec_context);
	}

	return OK;
}

static int32_t KYTY_SYSV_ABI GetPictureInfo(const Videodec2OutputInfo* output_info,
                                            void* /*first_picture_info*/,
                                            void* /*second_picture_info*/) {
	PRINT_NAME();

	if (output_info == nullptr) {
		return VIDEODEC2_ERROR_ARGUMENT_POINTER;
	}

	if (!IsOutputInfoSizeValid(output_info->this_size)) {
		return VIDEODEC2_ERROR_STRUCT_SIZE;
	}

	return OK;
}

LIB_DEFINE(InitVideoDec2_1) {
	PRINT_NAME_ENABLE(true);

	LIB_FUNC("RnDibcGCPKw", QueryComputeMemoryInfo);
	LIB_FUNC("eD+X2SmxUt4", AllocateComputeQueue);
	LIB_FUNC("UvtA3FAiF4Y", ReleaseComputeQueue);
	LIB_FUNC("qqMCwlULR+E", QueryDecoderMemoryInfo);
	LIB_FUNC("CNNRoRYd8XI", CreateDecoder);
	LIB_FUNC("jwImxXRGSKA", DeleteDecoder);
	LIB_FUNC("852F5+q6+iM", Decode);
	LIB_FUNC("l1hXwscLuCY", Flush);
	LIB_FUNC("wJXikG6QFN8", Reset);
	LIB_FUNC("NtXRa3dRzU0", GetPictureInfo);
	LIB_FUNC("kjrLbcyhEiw", GetPictureInfo);
}

} // namespace VideoDec2

} // namespace Libs
