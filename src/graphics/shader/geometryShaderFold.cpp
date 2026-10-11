#include "graphics/shader/geometryShaderFold.h"

#include <utility>

namespace Libs::Graphics {
namespace {

constexpr uint32_t EsWords = 0x1a0 / sizeof(uint32_t);
constexpr uint32_t GsWords = 0x644 / sizeof(uint32_t);

uint64_t CodeFingerprint(std::span<const uint32_t> code) {
	uint64_t hash = 14695981039346656037ull;
	for (uint32_t word: code) {
		for (uint32_t shift = 0; shift < 32; shift += 8) {
			hash = (hash ^ ((word >> shift) & 0xffu)) * 1099511628211ull;
		}
	}
	return hash;
}

constexpr uint32_t MoveVector(uint32_t dst, uint32_t src) {
	return 0x7e000200u | (dst << 17u) | src;
}

} // namespace

bool CanFoldTriangleCopyGeometry(std::span<const uint32_t> es,
                                 std::span<const uint32_t> gs) {
	return es.size() >= EsWords && gs.size() >= GsWords &&
	       CodeFingerprint(es.first(EsWords)) == 0xa11d95e12904607full &&
	       CodeFingerprint(gs.first(GsWords)) == 0x5030f45ea420014aull;
}

bool FoldTriangleCopyGeometry(std::span<const uint32_t> es,
                              std::span<const uint32_t> gs, bool export_layer,
                              std::vector<uint32_t>& vertex_code) {
	if (!CanFoldTriangleCopyGeometry(es, gs)) {
		return false;
	}

	std::vector<uint32_t> code(es.begin(), es.begin() + EsWords - 1);
	// Host vertex invocations already represent the live input vertices. ES's
	// NGG wave-count EXEC setup and wave/lane numbering must not be reused: the
	// host subgroup layout is unrelated to the guest primitive subgroup.
	code[0x04 / 4] = 0xbf800000u; // s_nop
	code[0x08 / 4] = 0xbefe04c1u; // s_mov_b64 exec, -1
	code[0x0c / 4] = 0xbf800000u;
	code[0x10 / 4] = MoveVector(7, 128); // v7 = 0; overwritten by position fetch
	code[0x14 / 4] = 0xbf800000u;
	code[0xa0 / 4] = MoveVector(9, 128); // wave-local index is no longer needed
	code[0xa4 / 4] = 0xbf800000u;
	code[0x11c / 4] = 0xbeea0380u; // s_mov_b32 vcc_lo, 0 (guest wave index)
	code[0x120 / 4] = 0xbf800000u;

	// The four ES LDS writes only communicate attributes to GS. All seven
	// values are still in registers at the return, so export those directly.
	// Removing LDS also avoids indexing a fixed host allocation with a guest
	// vertex/baseVertex value, and needs no cross-invocation synchronization.
	for (uint32_t offset: {0x160u, 0x168u, 0x184u, 0x190u}) {
		code[offset / 4] = 0xbf800000u;
		code[offset / 4 + 1] = 0xbf800000u;
	}
	// GS exports ES record offsets 8/12 as position XY and offsets 0/4
	// as PARAM0.xy; the transformed first fetch is the texture coordinate.
	constexpr uint32_t epilogue[] = {
	    0xf80000cfu, 0x0c0b0a09u, // exp POS0, v9,v10,v11,v12
	    0xf8000203u, 0x00000706u, // exp PARAM0.xy, v6,v7
	};
	code.insert(code.end(), std::begin(epilogue), std::end(epilogue));
	if (export_layer) {
		// GS writes its layer value as a raw integer in POS1.z, not gl_Position.
		code.push_back(0x361010ffu); // v_and_b32 v8, 0xffff, v8
		code.push_back(0x0000ffffu);
		code.push_back(0xf80008d4u); // exp POS1.z, v8
		code.push_back(0x00080000u);
	}
	code.push_back(0xbf810000u);
	vertex_code = std::move(code);
	return true;
}

} // namespace Libs::Graphics
