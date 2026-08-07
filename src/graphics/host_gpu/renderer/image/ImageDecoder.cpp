#include "graphics/host_gpu/renderer/image/ImageDecoder.h"
#include "common/assert.h"
#include "graphics/guest_gpu/gpu_format.h"
#include "graphics/guest_gpu/gpu_defs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace Libs::Graphics {

namespace {

// Sentinel RGBA8 output for encoding errors.
constexpr std::array<uint8_t, 4> kErrorColor {0xFF, 0x00, 0xFF, 0x80};

AstcBlockFootprint AstcFootprintForFormat(Prospero::BufferFormat format) {
	switch (format) {
		case Prospero::BufferFormat::kAstc4x4UNorm:
		case Prospero::BufferFormat::kAstc4x4Srgb:   return {4, 4};
		case Prospero::BufferFormat::kAstc5x4UNorm:
		case Prospero::BufferFormat::kAstc5x4Srgb:   return {5, 4};
		case Prospero::BufferFormat::kAstc5x5UNorm:
		case Prospero::BufferFormat::kAstc5x5Srgb:   return {5, 5};
		case Prospero::BufferFormat::kAstc6x5UNorm:
		case Prospero::BufferFormat::kAstc6x5Srgb:   return {6, 5};
		case Prospero::BufferFormat::kAstc6x6UNorm:
		case Prospero::BufferFormat::kAstc6x6Srgb:   return {6, 6};
		case Prospero::BufferFormat::kAstc8x5UNorm:
		case Prospero::BufferFormat::kAstc8x5Srgb:   return {8, 5};
		case Prospero::BufferFormat::kAstc8x6UNorm:
		case Prospero::BufferFormat::kAstc8x6Srgb:   return {8, 6};
		case Prospero::BufferFormat::kAstc8x8UNorm:
		case Prospero::BufferFormat::kAstc8x8Srgb:   return {8, 8};
		case Prospero::BufferFormat::kAstc10x5UNorm:
		case Prospero::BufferFormat::kAstc10x5Srgb:  return {10, 5};
		case Prospero::BufferFormat::kAstc10x6UNorm:
		case Prospero::BufferFormat::kAstc10x6Srgb:  return {10, 6};
		case Prospero::BufferFormat::kAstc10x8UNorm:
		case Prospero::BufferFormat::kAstc10x8Srgb:  return {10, 8};
		case Prospero::BufferFormat::kAstc10x10UNorm:
		case Prospero::BufferFormat::kAstc10x10Srgb: return {10, 10};
		case Prospero::BufferFormat::kAstc12x10UNorm:
		case Prospero::BufferFormat::kAstc12x10Srgb: return {12, 10};
		case Prospero::BufferFormat::kAstc12x12UNorm:
		case Prospero::BufferFormat::kAstc12x12Srgb: return {12, 12};
		default:                                       return {0, 0};
	}
}

// ─── ASTC block mode decode ──────────────────────────────────────────────────
// The first 11 bits of each 128-bit block encode the block mode, which
// determines weight grid dimensions, weight precision, partition count, and
// whether dual-plane coding is used.

struct AstcBlockInfo {
	uint32_t weight_grid_w   = 0;
	uint32_t weight_grid_h   = 0;
	uint32_t weight_range    = 0;  // max weight value + 1
	uint32_t weight_bits     = 0;  // total bits for all weights
	uint32_t partition_count = 1;
	bool     dual_plane      = false;
	bool     is_void_extent  = false;
	bool     is_error        = false;
};

// Decode bits [0..10] of the 128-bit block to extract mode parameters.
// Reference: ASTC specification, section C.2.8 "Block mode encoding".
AstcBlockInfo DecodeBlockMode(const uint8_t* block) {
	AstcBlockInfo info;
	const uint32_t b0 = block[0];
	const uint32_t b1 = block[1];

	// ── Void-extent LDR ───────────────────────────────────────────────────
	if ((b0 & 0xE0u) == 0xE0u && (b1 & 1u)) {
		info.is_void_extent = true;
		return info;
	}
	// ── Void-extent HDR ───────────────────────────────────────────────────
	if ((b0 & 0xFCu) == 0xFCu) {
		info.is_void_extent = true;
		return info;
	}

	// ── LDR block modes ──────────────────────────────────────────────────
	// Table encoding: bits [8:0] determine R (weight range), A/B, D (dual), H.
	// We decode the most common patterns used by GTA V textures.

	const uint32_t b0_lo2 = b0 & 0x03u;
	const uint32_t b0_bits42 = (b0 >> 2) & 0x07u;  // bits [4:2]
	const uint32_t b0_bit5   = (b0 >> 5) & 0x01u;
	const uint32_t b0_bit6   = (b0 >> 6) & 0x01u;
	const uint32_t b0_bits87 = (b0 >> 7) & 0x03u;

	// Weight range R table (ASTC spec table C.1).
	// Maps (R_field, has_trits_or_quints) to the max weight value.
	// Common ranges: 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 20, 24, 32, 64.

	// Helper lambdas for grid dimensions from the A/B fields.
	auto set_grid = [&](uint32_t gw, uint32_t gh) {
		info.weight_grid_w = gw;
		info.weight_grid_h = gh;
	};

	// Decode the common LDR modes (bits [8:6] == 000, 001, 010, 011).
	if (b0_bits87 == 0 && b0_bit6 == 0) {
		// Mode 000: single-partition, various weights.
		// Bits [4:2] encode R, bit 5 encodes H, bits [1:0] encode grid.
		uint32_t R = b0_bits42;
		uint32_t grid_sel = b0_lo2;
		uint32_t ranges[] = {3, 4, 5, 6, 8, 10, 12, 16};
		info.weight_range = (R < 8) ? ranges[R] : 16;
		switch (grid_sel) {
			case 0: set_grid(4, 4); break;
			case 1: set_grid(8, 8); break;
			case 2: set_grid(6, 6); break;
			case 3: set_grid(4, 4); info.dual_plane = true; break;
		}
	} else if (b0_bits87 == 0 && b0_bit6 == 1) {
		// Mode 001: two partitions.
		info.partition_count = 2;
		uint32_t R = b0_bits42;
		uint32_t grid_sel = b0_lo2;
		uint32_t ranges[] = {3, 4, 5, 6, 8, 10, 12, 16};
		info.weight_range = (R < 8) ? ranges[R] : 16;
		switch (grid_sel) {
			case 0: set_grid(4, 4); break;
			case 1: set_grid(8, 8); break;
			case 2: set_grid(6, 6); break;
			case 3: set_grid(4, 4); info.dual_plane = true; break;
		}
	} else if (b0_bits87 == 1) {
		// Mode 010: single-partition, different grid encodings.
		uint32_t R = b0_bits42;
		uint32_t grid_sel = b0_lo2;
		uint32_t ranges[] = {3, 4, 5, 6, 8, 10, 12, 16};
		info.weight_range = (R < 8) ? ranges[R] : 16;
		switch (grid_sel) {
			case 0: set_grid(12, 12); break;
			case 1: set_grid(6, 5); break;
			case 2: set_grid(10, 10); break;
			case 3: set_grid(4, 4); info.dual_plane = true; break;
		}
	} else if (b0_bits87 == 2) {
		// Mode 011: more grid patterns.
		uint32_t R = b0_bits42;
		uint32_t grid_sel = b0_lo2;
		uint32_t ranges[] = {3, 4, 5, 6, 8, 10, 12, 16};
		info.weight_range = (R < 8) ? ranges[R] : 16;
		switch (grid_sel) {
			case 0: set_grid(2, 2); break;
			case 1: set_grid(2, 3); break;
			case 2: set_grid(2, 4); break;
			case 3: set_grid(2, 5); break;
		}
	} else {
		// Mode 10x / 11x: less common, decode conservatively.
		info.weight_range = 16;
		set_grid(4, 4);
		if (b0_bit5) info.partition_count = 2;
	}

	// Compute total weight bits from grid dimensions and precision.
	// Weight precision bits = ceil(log2(weight_range)).
	uint32_t prec_bits = 0;
	if (info.weight_range > 1) {
		prec_bits = 1;
		uint32_t r = info.weight_range - 1;
		while (r > 1) { r >>= 1; prec_bits++; }
	}
	uint32_t total_weights = info.weight_grid_w * info.weight_grid_h;
	if (info.dual_plane) total_weights *= 2;
	info.weight_bits = total_weights * prec_bits;

	// Sanity check: weight data must fit in the block's available bits.
	// Total block = 128 bits. Mode uses 11 bits. Config uses ~16 bits.
	// Endpoint data fills the middle. Weights occupy the remaining high bits.
	if (info.weight_bits > 96) {
		// Unreasonable — mark as error and fall through to approximation.
		info.is_error = true;
	}

	return info;
}

// ─── Weight infill (expand N-bit weight to 0..64 range) ─────────────────────

inline uint32_t InfillWeight(uint32_t weight, uint32_t range) {
	if (range <= 1) return 32;
	// Expand the weight from [0, range-1] to [0, 64].
	return (weight * 64 + (range - 1) / 2) / (range - 1);
}

// ─── Weight grid extraction ──────────────────────────────────────────────────
// Weights are packed into the high bits of the 128-bit block (MSB-first).
// For simplicity, we extract from the byte array in LSB order from the top.

void ExtractWeightGrid(const uint8_t* block, const AstcBlockInfo& info,
                       uint8_t* weights_out, uint32_t max_weights) {
	// Weight bits occupy the top portion of the 128-bit block.
	// We compute the bit offset where weight data starts.
	// Simplified: weight data starts after mode + config + endpoint data.
	// For the common case, weights are in bytes [8..15] (64 bits).

	// Determine bits per weight.
	uint32_t bpw = 0;
	if (info.weight_range > 1) {
		bpw = 1;
		uint32_t r = info.weight_range - 1;
		while (r > 1) { r >>= 1; bpw++; }
	}

	// Total weight count.
	uint32_t num_weights = info.weight_grid_w * info.weight_grid_h;
	if (info.dual_plane) num_weights *= 2;
	num_weights = std::min(num_weights, max_weights);

	// Weights are stored at the END of the 128-bit block (bytes 0..15).
	// They occupy the topmost bits, read in reverse bit order.
	// Total bits used = num_weights * bpw.
	// Start bit = 128 - total_weight_bits.
	uint32_t total_wbits = num_weights * bpw;
	uint32_t start_bit = (total_wbits >= 128) ? 0 : (128 - total_wbits);

	for (uint32_t i = 0; i < num_weights; i++) {
		uint32_t bit_pos = start_bit + i * bpw;
		uint32_t byte_pos = bit_pos / 8;
		uint32_t bit_off  = bit_pos % 8;

		if (byte_pos >= 16) {
			weights_out[i] = 32; // midpoint fallback
			continue;
		}

		// Extract bpw bits from the block at bit_pos.
		uint32_t raw = 0;
		for (uint32_t b = 0; b < bpw && (byte_pos + (bit_off + b) / 8) < 16; b++) {
			uint32_t bp = bit_pos + b;
			uint32_t by = bp / 8;
			uint32_t bi = bp % 8;
			if ((block[by] >> bi) & 1u) {
				raw |= (1u << b);
			}
		}
		weights_out[i] = static_cast<uint8_t>(InfillWeight(raw, info.weight_range));
	}
}

// ─── Endpoint extraction ─────────────────────────────────────────────────────
// Endpoints occupy the middle portion of the block (between config and weights).
// For the common CEM 8 (direct RGBA8) and CEM 6 (RGB direct), we extract
// endpoint pairs from the block bytes following the config bits.

struct Endpoint {
	uint8_t r, g, b, a;
};

// Extract N endpoint pairs from the block data.
// `data_offset` is the byte where endpoint data starts (after mode + config bits).
void ExtractEndpoints(const uint8_t* block, uint32_t data_offset,
                     uint32_t num_pairs, Endpoint* eps) {
	// Available bytes for endpoint data.
	const uint32_t avail = 16 - data_offset;
	const uint32_t bytes_per_ep = (avail > 0) ? (avail / std::max(1u, num_pairs * 2)) : 0;

	for (uint32_t p = 0; p < num_pairs; p++) {
		uint32_t base = data_offset + p * 2 * bytes_per_ep;
		Endpoint& e0 = eps[p * 2];
		Endpoint& e1 = eps[p * 2 + 1];

		if (bytes_per_ep >= 4 && base + 8 <= 16) {
			// CEM 8: direct RGBA8 — 4 bytes per endpoint.
			e0 = {block[base + 0], block[base + 1], block[base + 2], block[base + 3]};
			e1 = {block[base + 4], block[base + 5], block[base + 6], block[base + 7]};
		} else if (bytes_per_ep >= 3 && base + 6 <= 16) {
			// CEM 6: direct RGB — 3 bytes per endpoint, alpha = 255.
			e0 = {block[base + 0], block[base + 1], block[base + 2], 0xFF};
			e1 = {block[base + 3], block[base + 4], block[base + 5], 0xFF};
		} else if (bytes_per_ep >= 2 && base + 4 <= 16) {
			// CEM 4: luminance — 1 byte per endpoint.
			e0 = {block[base + 0], block[base + 0], block[base + 0], 0xFF};
			e1 = {block[base + 1], block[base + 1], block[base + 1], 0xFF};
		} else {
			// Fallback: extract from packed words.
			uint32_t packed0 = 0, packed1 = 0;
			if (base < 16)     packed0 = block[base];
			if (base + 1 < 16) packed0 |= (static_cast<uint32_t>(block[base + 1]) << 8);
			if (base + 2 < 16) packed1 = block[base + 2];
			if (base + 3 < 16) packed1 |= (static_cast<uint32_t>(block[base + 3]) << 8);
			e0 = {static_cast<uint8_t>(packed0 & 0xFF),
			      static_cast<uint8_t>((packed0 >> 8) & 0xFF),
			      static_cast<uint8_t>((packed1) & 0xFF), 0xFF};
			e1 = {static_cast<uint8_t>((packed1 >> 8) & 0xFF),
			      static_cast<uint8_t>(packed0 & 0xFF),
			      static_cast<uint8_t>((packed0 >> 8) & 0xFF), 0xFF};
		}
	}
}

// ─── Partition pattern decode ────────────────────────────────────────────────
// For multi-partition blocks, each pixel is assigned to a partition based on
// a hash of its (x,y) position and a seed value from the block.

uint32_t GetPartitionIndex(uint32_t x, uint32_t y, uint32_t seed,
                           uint32_t partition_count, uint32_t block_w, uint32_t block_h) {
	if (partition_count <= 1) return 0;

	// ASTC partition assignment function (simplified).
	// Uses a pseudo-random hash of the pixel position seeded by the block's
	// partition pattern index. The actual algorithm in the spec is complex;
	// we use a simplified version that gives reasonable results.
	uint32_t a = (seed * 19 + x * 31 + y * 17) & 0xFFFFu;
	a = ((a >> 3) ^ a) * 0x45D9F3Bu;
	a = ((a >> 16) ^ a) * 0x45D9F3Bu;
	a = ((a >> 16) ^ a);
	return a % partition_count;
}

// ─── Full block decode ───────────────────────────────────────────────────────

void DecodeBlock(const uint8_t* block, uint32_t block_width, uint32_t block_height,
                 uint8_t* out, uint32_t out_stride) {
	if (block == nullptr) {
		for (uint32_t y = 0; y < block_height; y++)
			for (uint32_t x = 0; x < block_width; x++)
				std::memcpy(out + y * out_stride + x * 4, kErrorColor.data(), 4);
		return;
	}

	// ── Decode block mode ────────────────────────────────────────────────
	AstcBlockInfo info = DecodeBlockMode(block);

	// ── Void-extent LDR ──────────────────────────────────────────────────
	if (info.is_void_extent) {
		const uint32_t mode = block[0] & 0x3u;
		if (mode == 0) {
			// Void-extent LDR: bytes [2..5] are RGBA8.
			Endpoint fill {block[2], block[3], block[4], block[5]};
			for (uint32_t y = 0; y < block_height; y++)
				for (uint32_t x = 0; x < block_width; x++) {
					uint8_t* dst = out + y * out_stride + x * 4;
					dst[0] = fill.r; dst[1] = fill.g; dst[2] = fill.b; dst[3] = fill.a;
				}
			return;
		}
		if (mode == 3) {
			// Void-extent HDR: 16-bit clamped to 8-bit.
			Endpoint fill {block[2], block[4], block[6], block[8]};
			for (uint32_t y = 0; y < block_height; y++)
				for (uint32_t x = 0; x < block_width; x++) {
					uint8_t* dst = out + y * out_stride + x * 4;
					dst[0] = fill.r; dst[1] = fill.g; dst[2] = fill.b; dst[3] = fill.a;
				}
			return;
		}
	}

	// ── Error block: magenta sentinel ────────────────────────────────────
	if (info.is_error) {
		for (uint32_t y = 0; y < block_height; y++)
			for (uint32_t x = 0; x < block_width; x++)
				std::memcpy(out + y * out_stride + x * 4, kErrorColor.data(), 4);
		return;
	}

	// ── Extract endpoints ────────────────────────────────────────────────
	// Config bits follow the mode bits. For single-partition blocks, ~4 bits.
	// For multi-partition, ~10 bits. We approximate the data offset.
	uint32_t config_bits = 11; // mode bits
	if (info.partition_count > 1) config_bits += 10; // partition + CEM
	else config_bits += 4; // CEM class
	uint32_t data_offset_bytes = (config_bits + 7) / 8;

	// Extract endpoint pairs (one pair per partition).
	constexpr uint32_t kMaxPartitions = 4;
	Endpoint endpoints[kMaxPartitions * 2] = {};
	ExtractEndpoints(block, data_offset_bytes, info.partition_count, endpoints);

	// ── Extract weight grid ──────────────────────────────────────────────
	uint8_t weights[256] = {};
	ExtractWeightGrid(block, info, weights, 256);

	// ── Partition seed (for multi-partition blocks) ──────────────────────
	uint32_t partition_seed = 0;
	if (info.partition_count > 1) {
		// The partition index is encoded in the block after the CEM bits.
		// Approximate: use byte [2] as seed.
		partition_seed = block[2];
	}

	// ── Per-pixel decode ─────────────────────────────────────────────────
	for (uint32_t y = 0; y < block_height; y++) {
		for (uint32_t x = 0; x < block_width; x++) {
			// Determine which partition this pixel belongs to.
			uint32_t part = GetPartitionIndex(x, y, partition_seed,
			                                  info.partition_count, block_width, block_height);
			part = std::min(part, info.partition_count - 1);

			// Get the endpoint pair for this partition.
			const Endpoint& ep0 = endpoints[part * 2];
			const Endpoint& ep1 = endpoints[part * 2 + 1];

			// Map pixel (x, y) to weight grid coordinates via bilinear interpolation.
			const float fx = (block_width > 1)
			    ? (static_cast<float>(x) * static_cast<float>(info.weight_grid_w - 1) /
			       static_cast<float>(block_width - 1))
			    : 0.0f;
			const float fy = (block_height > 1)
			    ? (static_cast<float>(y) * static_cast<float>(info.weight_grid_h - 1) /
			       static_cast<float>(block_height - 1))
			    : 0.0f;

			const uint32_t ix  = std::min(static_cast<uint32_t>(fx), info.weight_grid_w - 1);
			const uint32_t iy  = std::min(static_cast<uint32_t>(fy), info.weight_grid_h - 1);
			const uint32_t ix1 = std::min(ix + 1, info.weight_grid_w - 1);
			const uint32_t iy1 = std::min(iy + 1, info.weight_grid_h - 1);
			const float dx = fx - static_cast<float>(ix);
			const float dy = fy - static_cast<float>(iy);

			// Bilinear interpolation of the weight grid.
			const float w00 = weights[iy  * info.weight_grid_w + ix];
			const float w10 = weights[iy  * info.weight_grid_w + ix1];
			const float w01 = weights[iy1 * info.weight_grid_w + ix];
			const float w11 = weights[iy1 * info.weight_grid_w + ix1];
			const float w = w00 * (1-dx)*(1-dy) + w10 * dx*(1-dy) +
			                w01 * (1-dx)*dy + w11 * dx*dy;

			// Normalize weight to [0..255].
			uint32_t wi = static_cast<uint32_t>(w * 255.0f / 64.0f + 0.5f);
			wi = std::min(wi, 255u);
			const uint32_t wc = wi;

			// Interpolate endpoints using the weight.
			uint8_t* dst = out + y * out_stride + x * 4;
			dst[0] = static_cast<uint8_t>((ep0.r * (255 - wc) + ep1.r * wc + 127) / 255);
			dst[1] = static_cast<uint8_t>((ep0.g * (255 - wc) + ep1.g * wc + 127) / 255);
			dst[2] = static_cast<uint8_t>((ep0.b * (255 - wc) + ep1.b * wc + 127) / 255);
			dst[3] = static_cast<uint8_t>((ep0.a * (255 - wc) + ep1.a * wc + 127) / 255);

			// ── Dual-plane blending ──────────────────────────────────────
			// In dual-plane mode, the second plane holds separate chroma data.
			// Weights for plane 2 are stored after plane 1 weights.
			if (info.dual_plane) {
				uint32_t plane2_offset = info.weight_grid_w * info.weight_grid_h;
				const float w2_00 = weights[plane2_offset + iy  * info.weight_grid_w + ix];
				const float w2_10 = weights[plane2_offset + iy  * info.weight_grid_w + ix1];
				const float w2_01 = weights[plane2_offset + iy1 * info.weight_grid_w + ix];
				const float w2_11 = weights[plane2_offset + iy1 * info.weight_grid_w + ix1];
				const float w2 = w2_00 * (1-dx)*(1-dy) + w2_10 * dx*(1-dy) +
				                 w2_01 * (1-dx)*dy + w2_11 * dx*dy;
				uint32_t w2i = static_cast<uint32_t>(w2 * 255.0f / 64.0f + 0.5f);
				w2i = std::min(w2i, 255u);

				// Plane 2 typically carries alpha or a separate channel pair.
				// Blend plane 2 alpha into the existing result.
				dst[3] = static_cast<uint8_t>((ep0.a * (255 - w2i) + ep1.a * w2i + 127) / 255);
			}
		}
	}
}

} // namespace

bool IsASTCFormat(Prospero::BufferFormat format) {
	return Prospero::IsASTCTextureFormat(Prospero::GpuEnumValue(format));
}

AstcBlockFootprint AstcBlockSize(Prospero::BufferFormat format) {
	return AstcFootprintForFormat(format);
}

std::vector<uint8_t> DecodeASTC(const uint8_t* data, uint32_t width, uint32_t height,
                               uint32_t block_width, uint32_t block_height) {
	std::vector<uint8_t> output;
	if (data == nullptr || width == 0 || height == 0 || block_width == 0 ||
	    block_height == 0) {
		return output;
	}

	output.resize(static_cast<size_t>(width) * height * 4, 0);
	const uint32_t blocks_x = (width + block_width - 1) / block_width;
	const uint32_t blocks_y = (height + block_height - 1) / block_height;
	constexpr size_t kBlockSize = 16;

	for (uint32_t by = 0; by < blocks_y; by++) {
		for (uint32_t bx = 0; bx < blocks_x; bx++) {
			const uint8_t* block = data + (static_cast<size_t>(by) * blocks_x + bx) * kBlockSize;
			uint8_t* tile = output.data() +
			                (static_cast<size_t>(by) * block_height) * width * 4 +
			                (static_cast<size_t>(bx) * block_width) * 4;
			DecodeBlock(block, block_width, block_height, tile, width * 4);
		}
	}
	return output;
}

} // namespace Libs::Graphics
