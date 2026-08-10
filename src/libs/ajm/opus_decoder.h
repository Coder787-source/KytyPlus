#pragma once

#include "common/logging/log.h"
#include "libs/ajm/ffmpeg_decoder_common.h"

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace Libs::Audio::Ajm {

class AjmOpusDecoder final: public AjmDecoder {
public:
	AjmOpusDecoder(uint32_t channels, uint32_t sample_rate, AjmSampleEncoding encoding,
	              uint64_t flags)
	    : AjmDecoder(channels, sample_rate, encoding) {
		(void)flags;

		m_codec = avcodec_find_decoder(AV_CODEC_ID_OPUS);
		if (m_codec == nullptr) {
			LOGF("AJM Opus: avcodec_find_decoder failed\n");
		}
	}

	~AjmOpusDecoder() override { Release(); }

	AjmDecodeResult Initialize(const void* codec_parameters,
	                           size_t      codec_parameters_size) override {
		auto result = MakeResult();

		if (codec_parameters == nullptr ||
		    codec_parameters_size < sizeof(AjmDecOpusInitializeParameters)) {
			result.result = AJM_RESULT_INVALID_PARAMETER;
			return result;
		}

		const auto* params = static_cast<const AjmDecOpusInitializeParameters*>(codec_parameters);
		if (params->channel_num == 0 || params->channel_num > AJM_DEC_OPUS_MAX_CHANNELS_FOR_10CH ||
		    params->sample_rate == 0) {
			result.result = AJM_RESULT_INVALID_PARAMETER;
			return result;
		}

		SetFormat(params->channel_num, params->sample_rate, m_sample_encoding);
		m_mapping_family        = params->mapping_family;
		m_total_decoded_samples = 0;
		m_frames_per_packet     = 1;
		m_is_initialized        = true;

		Reset();

		if (m_codec_context == nullptr) {
			m_is_initialized = false;
			result.result    = AJM_RESULT_CODEC_ERROR | AJM_RESULT_FATAL;
			return result;
		}

		LOGF("AJM Opus initialized: %" PRIu32 " Hz, %" PRIu32 " ch, mapping=%" PRIu32 "\n",
		     params->sample_rate, params->channel_num, params->mapping_family);

		return result;
	}

	void Reset() override {
		Release();
		m_total_decoded_samples = 0;
		if (m_is_initialized) {
			OpenDecoder();
		}
	}

	AjmDecodeResult Decode(const void* input, size_t input_size, void* output, size_t output_size,
	                       bool multiple_frames, AjmGaplessState* gapless) override {
		(void)multiple_frames;
		auto result = MakeResult();

		if (!m_is_initialized) {
			result.result = AJM_RESULT_NOT_INITIALIZED;
			return result;
		}
		if (m_codec_context == nullptr) {
			result.result = AJM_RESULT_CODEC_ERROR | AJM_RESULT_FATAL;
			return result;
		}
		if (input == nullptr || input_size == 0) {
			result.result = AJM_RESULT_PARTIAL_INPUT;
			return result;
		}
		if (input_size > static_cast<size_t>(std::numeric_limits<int>::max())) {
			result.result = AJM_RESULT_INVALID_PARAMETER;
			return result;
		}

		size_t output_offset = 0;
		// Each AJM Decode call is fed exactly one Opus packet (the PS5 AJM ABI hands packets,
		// not raw Ogg pages), so the whole input buffer is a single packet to send.
		DecodePacket(static_cast<const uint8_t*>(input), static_cast<int>(input_size), output,
		            output_size, &output_offset, gapless, &result);

		if (result.frames == 0 && result.result == OK) {
			result.result = AJM_RESULT_PARTIAL_INPUT;
		}

		result.input_consumed        = input_size;
		result.output_written        = output_offset;
		result.frames_per_packet     = m_frames_per_packet;
		result.total_decoded_samples = m_total_decoded_samples;
		result.format                = GetFormat();
		return result;
	}

	void WriteCodecInfo(void* output, size_t output_size,
	                    const AjmDecodeResult& result) const override {
		(void)result;
		if (output == nullptr || output_size < sizeof(AjmSidebandDecOpusCodecInfo)) {
			return;
		}

		auto* info              = static_cast<AjmSidebandDecOpusCodecInfo*>(output);
		info->frames_per_packet = m_frames_per_packet;
	}

	[[nodiscard]] size_t CodecInfoSize() const override {
		return sizeof(AjmSidebandDecOpusCodecInfo);
	}

private:
	void Release() {
		if (m_codec_context != nullptr) {
			avcodec_free_context(&m_codec_context);
		}
	}

	void OpenDecoder() {
		if (m_codec == nullptr) {
			return;
		}

		m_codec_context = avcodec_alloc_context3(m_codec);
		if (m_codec_context == nullptr) {
			return;
		}

		m_codec_context->sample_rate = static_cast<int>(m_sample_rate);
		av_channel_layout_default(&m_codec_context->ch_layout, static_cast<int>(m_channels));

		if (avcodec_open2(m_codec_context, m_codec, nullptr) < 0) {
			LOGF("AJM Opus: decoder initialization failed\n");
			Release();
		}
	}

	bool WriteFrame(const AVFrame* frame, void* output, size_t output_size, size_t* output_offset,
	                AjmGaplessState* gapless, AjmDecodeResult* result) {
		if (frame == nullptr || output_offset == nullptr || result == nullptr) {
			return false;
		}

		const auto channels = static_cast<uint32_t>(std::max(frame->ch_layout.nb_channels, 1));
		const auto bpf      = channels * AjmBytesPerSample(m_sample_encoding);
		if (bpf == 0) {
			result->result = AJM_RESULT_INVALID_PARAMETER;
			return false;
		}

		SetFormat(channels, static_cast<uint32_t>(std::max(frame->sample_rate, 1)),
		          m_sample_encoding);

		result->frames++;

		uint32_t skip_samples = 0;
		if (gapless != nullptr && gapless->current.skip_samples > 0) {
			skip_samples = std::min<uint32_t>(frame->nb_samples, gapless->current.skip_samples);
			gapless->current.skip_samples =
			    static_cast<uint16_t>(gapless->current.skip_samples - skip_samples);
		}

		uint32_t samples_to_write = static_cast<uint32_t>(frame->nb_samples) - skip_samples;
		if (gapless != nullptr && gapless->HasSampleLimit()) {
			samples_to_write = std::min(samples_to_write, gapless->current.total_samples);
		}

		const auto requested_bytes = static_cast<size_t>(samples_to_write) * bpf;
		auto       writable_bytes  = requested_bytes;
		if (output == nullptr || *output_offset >= output_size) {
			writable_bytes = 0;
		} else {
			writable_bytes = std::min(writable_bytes, output_size - *output_offset);
			writable_bytes -= writable_bytes % bpf;
		}

		if (writable_bytes != 0) {
			auto*       dst = static_cast<uint8_t*>(output) + *output_offset;
			const auto* src = frame->data[0] + static_cast<size_t>(skip_samples) * bpf;
			std::memcpy(dst, src, writable_bytes);
			*output_offset += writable_bytes;
		}

		const auto samples_written = static_cast<uint32_t>(writable_bytes / bpf);
		if (gapless != nullptr && gapless->HasSampleLimit()) {
			gapless->current.total_samples -=
			    std::min(gapless->current.total_samples, samples_written);
		}
		if (gapless != nullptr) {
			const auto skipped_total =
			    std::min<uint32_t>(std::numeric_limits<uint16_t>::max(),
			                       static_cast<uint32_t>(gapless->current.skipped_samples) +
			                           skip_samples + (samples_to_write - samples_written));
			gapless->current.skipped_samples = static_cast<uint16_t>(skipped_total);
		}

		m_total_decoded_samples += samples_written;
		result->total_decoded_samples = m_total_decoded_samples;
		result->format                = GetFormat();

		if (writable_bytes != requested_bytes) {
			result->result |= AJM_RESULT_NOT_ENOUGH_ROOM;
			return false;
		}
		return true;
	}

	bool DecodePacket(const uint8_t* packet_data, int packet_size, void* output, size_t output_size,
	                  size_t* output_offset, AjmGaplessState* gapless, AjmDecodeResult* result) {
		if (packet_data == nullptr || packet_size <= 0 || result == nullptr) {
			return true;
		}

		AVPacket* packet = av_packet_alloc();
		if (packet == nullptr || av_new_packet(packet, packet_size) < 0) {
			av_packet_free(&packet);
			result->result = AJM_RESULT_CODEC_ERROR | AJM_RESULT_FATAL;
			return false;
		}
		std::memcpy(packet->data, packet_data, static_cast<size_t>(packet_size));

		int rc = avcodec_send_packet(m_codec_context, packet);
		av_packet_free(&packet);
		if (rc == AVERROR_INVALIDDATA) {
			result->result = AJM_RESULT_CODEC_ERROR | AJM_RESULT_INVALID_DATA;
			return false;
		}
		if (rc < 0) {
			result->result = AJM_RESULT_PARTIAL_INPUT;
			return false;
		}

		for (;;) {
			AVFrame* frame = av_frame_alloc();
			if (frame == nullptr) {
				result->result = AJM_RESULT_CODEC_ERROR | AJM_RESULT_FATAL;
				return false;
			}

			rc = avcodec_receive_frame(m_codec_context, frame);
			if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) {
				av_frame_free(&frame);
				return true;
			}
			if (rc < 0) {
				av_frame_free(&frame);
				result->result = AJM_RESULT_CODEC_ERROR | AJM_RESULT_INVALID_DATA;
				return false;
			}

			AVFrame* converted = AjmConvertFfmpegFrame(frame, m_sample_encoding, result);
			av_frame_free(&frame);
			if (converted == nullptr) {
				return false;
			}

			const bool ok =
			    WriteFrame(converted, output, output_size, output_offset, gapless, result);
			av_frame_free(&converted);
			if (!ok) {
				return false;
			}
		}
	}

	const AVCodec*  m_codec             = nullptr;
	AVCodecContext* m_codec_context     = nullptr;
	uint32_t        m_mapping_family    = 0;
	uint32_t        m_frames_per_packet = 1;
	bool            m_is_initialized    = false;
};

} // namespace Libs::Audio::Ajm
