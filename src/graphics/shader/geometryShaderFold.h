#ifndef EMULATOR_GRAPHICS_GEOMETRY_SHADER_FOLD_H_
#define EMULATOR_GRAPHICS_GEOMETRY_SHADER_FOLD_H_

#include <cstdint>
#include <span>
#include <vector>

namespace Libs::Graphics {

// Only this fully inspected pair is supported: ES writes XY/UV/Z/W/layer to LDS,
// and GS copies and compacts input triangles without changing their attributes.
// Unknown programs must still use the unsupported-geometry guard.
bool CanFoldTriangleCopyGeometry(std::span<const uint32_t> es,
                                 std::span<const uint32_t> gs);
bool FoldTriangleCopyGeometry(std::span<const uint32_t> es,
                              std::span<const uint32_t> gs, bool export_layer,
                              std::vector<uint32_t>& vertex_code);

} // namespace Libs::Graphics

#endif
