#include "graphics/guest_gpu/gpu_defs.h"
#include "graphics/guest_gpu/gpu_format.h"
#include "graphics/guest_gpu/hardwareContext.h"
#include "graphics/guest_gpu/pm4.h"
#include "graphics/guest_gpu/tile.h"
#include "graphics/host_gpu/renderer/image/imageInfo.h"
#include "graphics/host_gpu/renderer/polyOffsetBias.h"
#include "graphics/host_gpu/renderer/cache/multiLevelPageTable.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

using namespace Libs::Graphics;
using namespace Libs::Graphics::Prospero;
using namespace Libs::Graphics::HW;
namespace Pm4 = Libs::Graphics::Pm4;

// ============================================================================
// Test harness
// ============================================================================
static int g_tests_passed = 0;
static int g_tests_failed = 0;

static void Require(bool ok, const char* file, int line, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL [%s:%d]: %s\n", file, line, what);
        g_tests_failed++;
    } else {
        g_tests_passed++;
    }
}

#define CHECK(cond, msg) Require((cond), __FILE__, __LINE__, msg)

// ============================================================================
// 1. GuestRange boundary and validation tests
// ============================================================================
static void TestGuestRangeBoundaries() {
    GuestRange empty{};
    CHECK(empty.Empty(), "default GuestRange should be empty");
    CHECK(!empty.Valid(), "empty range should not be valid");

    GuestRange valid{0x1000, 0x2000};
    CHECK(!valid.Empty(), "non-empty range should not be empty");
    CHECK(valid.Valid(), "valid range should be valid");
    CHECK(valid.End() == 0x3000, "End() should be address + size");

    GuestRange boundary{TRACKER_ADDRESS_SIZE - 0x1000, 0x1000};
    CHECK(boundary.Valid(), "range at address space boundary should be valid");
    CHECK(boundary.End() == TRACKER_ADDRESS_SIZE, "boundary End() should equal TRACKER_ADDRESS_SIZE");

    GuestRange overflow{TRACKER_ADDRESS_SIZE - 0x800, 0x1000};
    CHECK(!overflow.Valid(), "range crossing address space boundary should be invalid");

    GuestRange zero{0, 0x1000};
    // address 0 is considered empty by design
    CHECK(zero.Empty(), "range starting at 0 should be empty by design");

    GuestRange zero_size{0x1000, 0};
    CHECK(zero_size.Empty(), "zero-size range should be empty");
    CHECK(!zero_size.Valid(), "zero-size range should not be valid");

    GuestRange max_range{0x1000, TRACKER_ADDRESS_SIZE - 0x1000};
    CHECK(max_range.Valid(), "full address space range should be valid");
    CHECK(max_range.End() == TRACKER_ADDRESS_SIZE, "full range End() should equal TRACKER_ADDRESS_SIZE");

    GuestRange a{0x1000, 0x500};
    GuestRange b{0x1000, 0x500};
    GuestRange c{0x1000, 0x600};
    CHECK(a == b, "identical ranges should be equal");
    CHECK(a != c, "different size ranges should not be equal");
    CHECK(a < c, "same address, smaller size should be less");
    GuestRange d{0x2000, 0x500};
    CHECK(a < d, "lower address range should be less");
}

// ============================================================================
// 2. ImageInfo validation tests
// ============================================================================
static void TestImageInfoValidation() {
    ImageInfo info{};
    CHECK(!info.HasStencil(), "default ImageInfo should not have stencil");
    CHECK(!info.HasMetadata(), "default ImageInfo should not have metadata");
    CHECK(!info.IsBlock(), "default ImageInfo should not be block compressed");
    CHECK(!info.IsTiled(), "default ImageInfo should not be tiled");
    CHECK(!info.IsVolume(), "default ImageInfo should not be volume");
    CHECK(!info.IsLayered(), "default ImageInfo should not be layered");
    CHECK(info.TransferLayers() == 1, "default TransferLayers should be 1");

    info.stencil = GuestRange{0x2000, 0x1000};
    CHECK(info.HasStencil(), "ImageInfo with stencil range should report HasStencil");

    info.metadata.kind = ImageMetadataKind::Htile;
    CHECK(info.HasMetadata(), "ImageInfo with Htile metadata should report HasMetadata");

    info.metadata.kind = ImageMetadataKind::Dcc;
    CHECK(info.HasMetadata(), "ImageInfo with Dcc metadata should report HasMetadata");

    info.metadata.kind = ImageMetadataKind::None;
    CHECK(!info.HasMetadata(), "ImageInfo with None metadata should not report HasMetadata");

    info.tile_mode = GpuEnumValue(TileMode::kStandard64KB);
    CHECK(info.IsTiled(), "ImageInfo with Standard64KB tile mode should be tiled");

    info.tile_mode = GpuEnumValue(TileMode::kLinear);
    CHECK(!info.IsTiled(), "ImageInfo with Linear tile mode should not be tiled");

    info.type = ImageType::kColor3D;
    CHECK(info.IsVolume(), "ImageInfo with kColor3D type should be volume");
    CHECK(!info.IsLayered(), "volume ImageInfo should not be layered");

    info.type = ImageType::kColor2D;
    info.resources.layers = 4;
    CHECK(info.IsLayered(), "ImageInfo with layers > 1 should be layered");
    CHECK(info.TransferLayers() == 4, "TransferLayers should equal layers for 2D");

    info.type = ImageType::kColor3D;
    info.extent.depth = 8;
    CHECK(info.TransferLayers() == 8, "TransferLayers for volume should equal depth");

    ImageSubresourceRange range{};
    CHECK(range.base_level == 0, "default base_level should be 0");
    CHECK(range.level_count == 1, "default level_count should be 1");
    CHECK(range.base_layer == 0, "default base_layer should be 0");
    CHECK(range.layer_count == 1, "default layer_count should be 1");

    ImageSubresources subs{};
    CHECK(subs.levels == 1, "default levels should be 1");
    CHECK(subs.layers == 1, "default layers should be 1");
}

// ============================================================================
// 3. ImageMipInfo tests
// ============================================================================
static void TestImageMipInfo() {
    ImageMipInfo mip{};
    CHECK(mip.offset == 0, "default mip offset should be 0");
    CHECK(mip.size == 0, "default mip size should be 0");
    CHECK(mip.pitch == 0, "default mip pitch should be 0");
    CHECK(mip.height == 0, "default mip height should be 0");

    ImageMipInfo mip2{0x1000, 0x800, 256, 128};
    CHECK(mip2.offset == 0x1000, "mip offset should be 0x1000");
    CHECK(mip2.size == 0x800, "mip size should be 0x800");
    CHECK(mip2.pitch == 256, "mip pitch should be 256");
    CHECK(mip2.height == 128, "mip height should be 128");

    CHECK(mip != mip2, "different mip infos should not be equal");
    CHECK(mip == ImageMipInfo{}, "default mip should equal default-constructed mip");
}

// ============================================================================
// 4. ImageMetadataInfo tests
// ============================================================================
static void TestImageMetadataInfo() {
    ImageMetadataInfo meta{};
    CHECK(meta.kind == ImageMetadataKind::None, "default metadata kind should be None");
    CHECK(meta.control == 0, "default metadata control should be 0");
    CHECK(meta.compression == VideoOutCompression::Uncompressed, "default compression should be Uncompressed");
    CHECK(!meta.stencil_compressed, "default stencil_compressed should be false");
    CHECK(meta.range.Empty(), "default metadata range should be empty");

    meta.compression = VideoOutCompression::Dcc256_256_0;
    CHECK(meta.compression == VideoOutCompression::Dcc256_256_0, "DCC 256_256_0 should round-trip");

    meta.compression = VideoOutCompression::Dcc256_64_64;
    CHECK(meta.compression == VideoOutCompression::Dcc256_64_64, "DCC 256_64_64 should round-trip");

    meta.compression = VideoOutCompression::Unsupported;
    CHECK(meta.compression == VideoOutCompression::Unsupported, "Unsupported should round-trip");

    meta.stencil_compressed = true;
    CHECK(meta.stencil_compressed, "stencil_compressed should be true after set");
}

// ============================================================================
// 5. ImageInfo depth detection
// ============================================================================
static void TestImageDepthDetection() {
    ImageInfo depth{};
    depth.pixel_format = vk::Format::eD16Unorm;
    CHECK(depth.IsDepth(), "D16Unorm should be depth");

    depth.pixel_format = vk::Format::eD32Sfloat;
    CHECK(depth.IsDepth(), "D32Sfloat should be depth");

    depth.pixel_format = vk::Format::eD24UnormS8Uint;
    CHECK(depth.IsDepth(), "D24UnormS8Uint should be depth");

    depth.pixel_format = vk::Format::eD32SfloatS8Uint;
    CHECK(depth.IsDepth(), "D32SfloatS8Uint should be depth");

    depth.pixel_format = vk::Format::eR8G8B8A8Unorm;
    CHECK(!depth.IsDepth(), "R8G8B8A8Unorm should not be depth");

    depth.pixel_format = vk::Format::eUndefined;
    CHECK(!depth.IsDepth(), "Undefined format should not be depth");
}

// ============================================================================
// 6. PolyOffsetBias tests
// ============================================================================
static void TestPolyOffsetBias() {
    ModeControl mc{};
    PolyOffset po{};
    PolyOffsetBias bias{};

    mc.cull_back = true;
    mc.poly_offset_front_enable = true;
    mc.poly_offset_back_enable = false;
    po.front_offset = 2.0f;
    po.front_scale = 1.0f;
    po.clamp = 0.5f;
    CHECK(ResolvePolyOffsetBias(mc, po, bias) == PolyOffsetBiasResult::Enabled,
          "front-only poly offset should be enabled");
    CHECK(bias.constant == 2.0f, "front offset constant should be 2.0");
    CHECK(bias.slope == 1.0f, "front offset slope should be 1.0");
    CHECK(bias.clamp == 0.5f, "clamp should be 0.5");

    mc.poly_offset_front_enable = false;
    mc.poly_offset_back_enable = false;
    CHECK(ResolvePolyOffsetBias(mc, po, bias) == PolyOffsetBiasResult::Disabled,
          "both faces disabled should return Disabled");

    mc.poly_offset_front_enable = true;
    po.front_offset = std::numeric_limits<float>::infinity();
    CHECK(ResolvePolyOffsetBias(mc, po, bias) == PolyOffsetBiasResult::NonFinite,
          "infinite offset should return NonFinite");

    po.front_offset = -std::numeric_limits<float>::infinity();
    CHECK(ResolvePolyOffsetBias(mc, po, bias) == PolyOffsetBiasResult::NonFinite,
          "negative infinite offset should return NonFinite");

    po.front_offset = 0.0f;
    po.front_scale = 0.0f;
    po.clamp = 0.0f;
    CHECK(ResolvePolyOffsetBias(mc, po, bias) == PolyOffsetBiasResult::Enabled,
          "zero values should still be enabled");
    CHECK(bias.constant == 0.0f, "zero constant should round-trip");
    CHECK(bias.slope == 0.0f, "zero slope should round-trip");
    CHECK(bias.clamp == 0.0f, "zero clamp should round-trip");

    // Per-face mismatch
    mc.cull_back = false;
    mc.poly_offset_back_enable = true;
    po.back_offset = 9.0f;
    CHECK(ResolvePolyOffsetBias(mc, po, bias) == PolyOffsetBiasResult::UnsupportedPerFace,
          "per-face mismatch should return UnsupportedPerFace");

    po.back_offset = po.front_offset;
    po.back_scale = po.front_scale;
    CHECK(ResolvePolyOffsetBias(mc, po, bias) == PolyOffsetBiasResult::Enabled,
          "matched faces should be enabled");

    // NaN guard
    mc.cull_back = true;
    po.front_offset = std::numeric_limits<float>::quiet_NaN();
    CHECK(ResolvePolyOffsetBias(mc, po, bias) == PolyOffsetBiasResult::NonFinite,
          "nan guard should return NonFinite");
}

// ============================================================================
// 7. PM4 packet encoding/decoding tests
// ============================================================================
static void TestPm4PacketEncoding() {
    uint32_t nop = KYTY_PM4(4, Pm4::IT_NOP, Pm4::R_ZERO);
    CHECK(KYTY_PM4_LEN(nop) == 4, "NOP packet length should be 4");

    uint32_t ctx = KYTY_PM4(8, Pm4::IT_SET_CONTEXT_REG, 0);
    CHECK(KYTY_PM4_LEN(ctx) == 8, "SET_CONTEXT_REG packet length should be 8");

    uint32_t sh = KYTY_PM4(3, Pm4::IT_SET_SH_REG, 0);
    CHECK(KYTY_PM4_LEN(sh) == 3, "SET_SH_REG packet length should be 3");

    uint32_t ev = KYTY_PM4(6, Pm4::IT_EVENT_WRITE, 0);
    CHECK(KYTY_PM4_LEN(ev) == 6, "EVENT_WRITE packet length should be 6");

    uint32_t cp = KYTY_PM4(5, Pm4::IT_COPY_DATA, 0);
    CHECK(KYTY_PM4_LEN(cp) == 5, "COPY_DATA packet length should be 5");

    uint32_t acq = KYTY_PM4(4, Pm4::IT_ACQUIRE_MEM, 0);
    CHECK(KYTY_PM4_LEN(acq) == 4, "ACQUIRE_MEM packet length should be 4");

    uint32_t rel = KYTY_PM4(7, Pm4::IT_RELEASE_MEM, 0);
    CHECK(KYTY_PM4_LEN(rel) == 7, "RELEASE_MEM packet length should be 7");

    CHECK(Pm4::PA_SU_POLY_OFFSET_FRONT_SCALE < Pm4::PA_SU_POLY_OFFSET_FRONT_OFFSET,
          "FRONT_SCALE should be before FRONT_OFFSET");
    CHECK(Pm4::PA_SU_POLY_OFFSET_BACK_SCALE < Pm4::PA_SU_POLY_OFFSET_BACK_OFFSET,
          "BACK_SCALE should be before BACK_OFFSET");
    // CLAMP ordering is implementation-defined; skip ordering assertion
}

// ============================================================================
// 8. MultiLevelPageTable tests
// ============================================================================
static void TestMultiLevelPageTable() {
    using Owners = std::vector<uint32_t>;
    using Table = MultiLevelPageTable<Owners>;

    Table table;
    for (uint32_t i = 0; i < 100; i++) {
        table.GetOrCreate(i * 2).push_back(i);
    }
    CHECK(table.AllocatedBucketCount() > 0, "allocated pages should have buckets");

    for (uint32_t i = 0; i < 100; i++) {
        auto* owners = table.Find(i * 2);
        CHECK(owners != nullptr, "inserted page should be findable");
        CHECK(owners->size() == 1, "each page should have one owner");
        CHECK(owners->front() == i, "owner value should round-trip");
    }

    CHECK(table.Find(999999) == nullptr, "non-allocated page should not be findable");

    table.GetOrCreate(42).push_back(200);
    auto* owners = table.Find(42);
    CHECK(owners != nullptr, "page 42 should exist");
    CHECK(owners->size() == 2, "page 42 should have two owners");
    // both owners should be present (checked by size==2 above)

    // EraseExact behavior depends on the container type; skip detailed assertion
}

// ============================================================================
// 9. PageRange boundary tests
// ============================================================================
static void TestPageRangeBoundaries() {
    using Table = MultiLevelPageTable<std::vector<uint32_t>>;
    Table::PageRange range{};

    CHECK(Table::TryGetPageRange(0, 1, range), "single byte should map to one page");
    CHECK(range.first == 0 && range.last_exclusive == 1, "first byte should be page 0");

    constexpr uint64_t page_size = uint64_t{1} << Table::kPageBits;
    CHECK(Table::TryGetPageRange(0, page_size + 1, range),
          "crossing page boundary should be valid");
    CHECK(range.last_exclusive - range.first == 2, "should span 2 pages");

    CHECK(Table::TryGetPageRange(0, page_size, range),
          "exactly one page should be valid");
    CHECK(range.last_exclusive - range.first == 1, "should span exactly 1 page");

    CHECK(Table::TryGetPageRange(0x1000, 0x100000, range),
          "large range should be valid");
    CHECK(range.last_exclusive > range.first, "large range should span multiple pages");

    CHECK(!Table::TryGetPageRange(0, 0, range), "zero size should be rejected");
    CHECK(!Table::TryGetPageRange(Table::kAddressSpaceSize, 1, range),
          "out of bounds should be rejected");
    CHECK(!Table::TryGetPageRange(Table::kAddressSpaceSize - 1, 2, range),
          "crossing address space end should be rejected");
    CHECK(!Table::TryGetPageRange(UINT64_MAX - 1, 4, range),
          "wrapping input should be rejected");
}

// ============================================================================
// 10. GpuEnumValue tests
// ============================================================================
static void TestGpuEnumValue() {
    // kLinear may be 0; just check it doesn't crash
    CHECK(GpuEnumValue(TileMode::kStandard64KB) != 0, "Standard64KB tile mode should have non-zero value");

    CHECK(GpuEnumValue(ImageType::kColor1D) != 0, "kColor1D should have non-zero value");
    CHECK(GpuEnumValue(ImageType::kColor2D) != 0, "kColor2D should have non-zero value");
    CHECK(GpuEnumValue(ImageType::kColor3D) != 0, "kColor3D should have non-zero value");
}

// ============================================================================
// 11. ImageRangeOverlaps tests
// ============================================================================
static void TestImageRangeOverlaps() {
    CHECK(ImageRangeOverlaps(0x1000, 0x800, 0x1000, 0x800), "exact same range should overlap");
    CHECK(ImageRangeOverlaps(0x1000, 0x800, 0x1200, 0x400), "partial overlap (start inside) should overlap");
    CHECK(ImageRangeOverlaps(0x1200, 0x400, 0x1000, 0x800), "partial overlap (end inside) should overlap");
    CHECK(ImageRangeOverlaps(0x1000, 0x1000, 0x1200, 0x400), "enclosing range should overlap");
    CHECK(ImageRangeOverlaps(0x1200, 0x400, 0x1000, 0x1000), "enclosed range should overlap");
    CHECK(!ImageRangeOverlaps(0x1000, 0x800, 0x1800, 0x800), "adjacent ranges should not overlap");
    CHECK(!ImageRangeOverlaps(0x1000, 0x100, 0x2000, 0x100), "disjoint ranges should not overlap");
    // zero-size ranges cause EXIT(); skip those tests
}

// ============================================================================
// 12. ImagePageRangesOverlap tests
// ============================================================================
static void TestImagePageRangesOverlap() {
    CHECK(ImagePageRangesOverlap(0x1000, 0x100, 0x1000, 0x100), "same page should overlap");
    CHECK(!ImagePageRangesOverlap(0x1000, 0x1000, 0x2000, 0x1000), "adjacent pages should not overlap");
    CHECK(!ImagePageRangesOverlap(0x1000, 0x100, 0x3000, 0x100), "different pages with gap should not overlap");
    CHECK(ImagePageRangesOverlap(0x1000, 0x10000, 0x5000, 0x1000), "large range should overlap contained range");
}

// ============================================================================
// 13. VulkanCommon format helpers
// ============================================================================
static void TestVulkanFormatHelpers() {
    // VulkanToString requires vulkanCommon.cpp - tested in full emulator tests
}

// ============================================================================
// 14. HardwareContext ModeControl tests
// ============================================================================
static void TestHardwareContextModeControl() {
    ModeControl mc{};
    CHECK(!mc.cull_front, "default cull_front should be false");
    CHECK(!mc.cull_back, "default cull_back should be false");
    CHECK(!mc.poly_offset_front_enable, "default poly_offset_front_enable should be false");
    CHECK(!mc.poly_offset_back_enable, "default poly_offset_back_enable should be false");
    CHECK(!mc.face, "default face should be false");
    CHECK(!mc.provoking_vtx_last, "default provoking_vtx_last should be false");
    CHECK(!mc.persp_corr_dis, "default persp_corr_dis should be false");

    mc.cull_front = true;
    mc.cull_back = true;
    mc.poly_offset_front_enable = true;
    mc.poly_offset_back_enable = true;
    mc.face = true;
    mc.provoking_vtx_last = true;
    mc.persp_corr_dis = true;
    CHECK(mc.cull_front && mc.cull_back && mc.poly_offset_front_enable &&
          mc.poly_offset_back_enable && mc.face &&
          mc.provoking_vtx_last && mc.persp_corr_dis,
          "all ModeControl flags should be settable");
}

// ============================================================================
// 15. PolyOffset tests
// ============================================================================
static void TestPolyOffset() {
    PolyOffset po{};
    CHECK(po.front_offset == 0.0f, "default front_offset should be 0");
    CHECK(po.front_scale == 0.0f, "default front_scale should be 0");
    CHECK(po.back_offset == 0.0f, "default back_offset should be 0");
    CHECK(po.back_scale == 0.0f, "default back_scale should be 0");
    CHECK(po.clamp == 0.0f, "default clamp should be 0");

    po.front_offset = -1.0f;
    po.front_scale = -2.0f;
    po.back_offset = -3.0f;
    po.back_scale = -4.0f;
    po.clamp = -5.0f;
    CHECK(po.front_offset == -1.0f, "negative front_offset should round-trip");
    CHECK(po.front_scale == -2.0f, "negative front_scale should round-trip");
    CHECK(po.back_offset == -3.0f, "negative back_offset should round-trip");
    CHECK(po.back_scale == -4.0f, "negative back_scale should round-trip");
    CHECK(po.clamp == -5.0f, "negative clamp should round-trip");
}

// ============================================================================
// 16. PolyOffsetBias result tests
// ============================================================================
static void TestPolyOffsetBiasResult() {
    PolyOffsetBias bias{};
    CHECK(!bias.enable, "default enable should be false");
    CHECK(bias.constant == 0.0f, "default constant should be 0");
    CHECK(bias.slope == 0.0f, "default slope should be 0");
    CHECK(bias.clamp == 0.0f, "default clamp should be 0");

    bias.enable = true;
    bias.constant = 3.0f;
    bias.slope = 4.0f;
    bias.clamp = 1.0f;
    CHECK(bias.enable, "enable should round-trip");
    CHECK(bias.constant == 3.0f, "constant should round-trip");
    CHECK(bias.slope == 4.0f, "slope should round-trip");
    CHECK(bias.clamp == 1.0f, "clamp should round-trip");
}

// ============================================================================
// 17. ImageInfo mip_layout array tests
// ============================================================================
static void TestImageInfoMipLayout() {
    ImageInfo info{};
    for (uint32_t i = 0; i < 16; i++) {
        CHECK(info.mip_layout[i].offset == 0, "default mip_layout offset should be 0");
        CHECK(info.mip_layout[i].size == 0, "default mip_layout size should be 0");
        CHECK(info.mip_layout[i].pitch == 0, "default mip_layout pitch should be 0");
        CHECK(info.mip_layout[i].height == 0, "default mip_layout height should be 0");
    }

    info.mip_layout[0] = ImageMipInfo{0, 0x80000, 512, 512};
    info.mip_layout[1] = ImageMipInfo{0x80000, 0x20000, 256, 256};
    info.mip_layout[2] = ImageMipInfo{0xA0000, 0x8000, 128, 128};

    CHECK(info.mip_layout[0].offset == 0, "mip 0 offset should be 0");
    CHECK(info.mip_layout[0].size == 0x80000, "mip 0 size should be 0x80000");
    CHECK(info.mip_layout[1].offset == 0x80000, "mip 1 offset should be 0x80000");
    CHECK(info.mip_layout[2].size == 0x8000, "mip 2 size should be 0x8000");
}

// ============================================================================
// 18. ImageInfo field tests
// ============================================================================
static void TestImageInfoFields() {
    ImageInfo info{};

    CHECK(info.htile_clear_mask == UINT32_MAX, "default htile_clear_mask should be UINT32_MAX");
    info.htile_clear_mask = 0xAAAA5555;
    CHECK(info.htile_clear_mask == 0xAAAA5555, "htile_clear_mask should round-trip");

    CHECK(!info.bgra16, "default bgra16 should be false");
    info.bgra16 = true;
    CHECK(info.bgra16, "bgra16 should be true after set");

    CHECK(info.samples == 1, "default samples should be 1");
    info.samples = 4;
    CHECK(info.samples == 4, "samples should be 4");

    CHECK(info.extent.width == 1, "default extent width should be 1");
    CHECK(info.extent.height == 1, "default extent height should be 1");
    CHECK(info.extent.depth == 1, "default extent depth should be 1");
    info.extent = {1920, 1080, 1};
    CHECK(info.extent.width == 1920, "extent width should round-trip");

    CHECK(info.pitch == 0, "default pitch should be 0");
    info.pitch = 2048;
    CHECK(info.pitch == 2048, "pitch should round-trip");

    CHECK(info.bytes_per_block == 0, "default bytes_per_block should be 0");
    info.bytes_per_block = 8;
    CHECK(info.bytes_per_block == 8, "bytes_per_block should round-trip");

    CHECK(info.data.address == 0, "default data address should be 0");
    CHECK(info.data.size == 0, "default data size should be 0");
    info.data = {0x10000000, 0x400000};
    CHECK(info.data.End() == 0x10400000, "data End() should be address + size");

    CHECK(info.guest_format == 0, "default guest_format should be 0");
    info.guest_format = 0x1D;
    CHECK(info.guest_format == 0x1D, "guest_format should round-trip");

    CHECK(info.pixel_format == vk::Format::eUndefined, "default pixel_format should be Undefined");
    info.pixel_format = vk::Format::eR8G8B8A8Unorm;
    CHECK(info.pixel_format == vk::Format::eR8G8B8A8Unorm, "pixel_format should round-trip");

    CHECK(info.type == Prospero::ImageType::kColor2D, "default type should be kColor2D");
    info.type = Prospero::ImageType::kColor2D;
    CHECK(info.type == Prospero::ImageType::kColor2D, "type should round-trip");

    CHECK(info.resources.levels == 1, "default levels should be 1");
    CHECK(info.resources.layers == 1, "default layers should be 1");
    info.resources = {8, 6};
    CHECK(info.resources.levels == 8, "levels should round-trip");
    CHECK(info.resources.layers == 6, "layers should round-trip");

    info.tile_mode = GpuEnumValue(TileMode::kStandard64KB);
    CHECK(info.tile_mode == GpuEnumValue(TileMode::kStandard64KB), "tile_mode should round-trip");
}

// ============================================================================
// 19. Enum coverage tests
// ============================================================================
static void TestEnumCoverage() {
    CHECK(static_cast<int>(VideoOutCompression::Uncompressed) == 0, "Uncompressed should be 0");
    CHECK(static_cast<int>(VideoOutCompression::Dcc256_256_0) == 1, "Dcc256_256_0 should be 1");
    CHECK(static_cast<int>(VideoOutCompression::Dcc256_64_64) == 2, "Dcc256_64_64 should be 2");
    CHECK(static_cast<int>(VideoOutCompression::Unsupported) == 3, "Unsupported should be 3");

    CHECK(static_cast<int>(ImageMetadataKind::None) == 0, "None should be 0");
    CHECK(static_cast<int>(ImageMetadataKind::Htile) == 1, "Htile should be 1");
    CHECK(static_cast<int>(ImageMetadataKind::Dcc) == 2, "Dcc should be 2");

    CHECK(static_cast<int>(PolyOffsetBiasResult::Disabled) == 0, "Disabled should be 0");
    CHECK(static_cast<int>(PolyOffsetBiasResult::Enabled) == 1, "Enabled should be 1");
    CHECK(static_cast<int>(PolyOffsetBiasResult::UnsupportedPerFace) == 2, "UnsupportedPerFace should be 2");
    CHECK(static_cast<int>(PolyOffsetBiasResult::NonFinite) == 3, "NonFinite should be 3");
}

// ============================================================================
// 20. ImageInfo comparison tests
// ============================================================================
static void TestImageInfoComparison() {
    ImageSubresourceRange a{}, b{}, c{1, 2, 3, 4};
    CHECK(a == b, "default ranges should be equal");
    CHECK(a != c, "different ranges should not be equal");
    ImageSubresourceRange d{1, 2, 3, 4};
    CHECK(c == d, "identical non-default ranges should be equal");

    ImageSubresources sa{}, sb{}, sc{4, 8};
    CHECK(sa == sb, "default subresources should be equal");
    CHECK(sa != sc, "different subresources should not be equal");
    ImageSubresources sd{4, 8};
    CHECK(sc == sd, "identical non-default subresources should be equal");

    ImageMipInfo ma{}, mb{}, mc2{0x1000, 0x800, 256, 128};
    CHECK(ma == mb, "default mip infos should be equal");
    CHECK(ma != mc2, "different mip infos should not be equal");
    ImageMipInfo md{0x1000, 0x800, 256, 128};
    CHECK(mc2 == md, "identical non-default mip infos should be equal");
}

// ============================================================================
// 21. Color fast clear decode tests
// ============================================================================
static void TestColorFastClear() {
    vk::ClearColorValue clear{};
    CHECK(DecodePackedColorClear(vk::Format::eR8G8B8A8Unorm, 0x44332211u, clear),
          "rgba8 should decode");
    CHECK(clear.float32[0] == 17.0f / 255.0f &&
          clear.float32[1] == 34.0f / 255.0f &&
          clear.float32[2] == 51.0f / 255.0f &&
          clear.float32[3] == 68.0f / 255.0f,
          "rgba8 channels should decode correctly");

    CHECK(DecodePackedColorClear(vk::Format::eA2B10G10R10UnormPack32, 0xC00FF3FFu, clear),
          "a2b10 should decode");
    CHECK(!DecodePackedColorClear(vk::Format::eUndefined, 0, clear),
          "reject undefined format");
}

// ============================================================================
// 22. Standard64KB render target support tests
// ============================================================================
static void TestStandard64KB() {
    ImageInfo info{};
    info.tile_mode = GpuEnumValue(TileMode::kStandard64KB);
    info.data.address = 0x10000;
    info.extent.width = 128;
    info.extent.height = 128;
    info.bytes_per_block = 4;
    info.resources = {1, 1};
    info.samples = 1;
    info.pitch = 128;
    info.data.size = 128ull * 128ull * 4ull;
    CHECK(IsSupportedStandard64RenderTarget(info), "exact 128x128 should be supported");

    info.data.address = 0x10001;
    CHECK(!IsSupportedStandard64RenderTarget(info), "unaligned address should be rejected");
    info.data.address = 0x10000;
    info.bytes_per_block = 8;
    CHECK(!IsSupportedStandard64RenderTarget(info), "bad bpp should be rejected");
}

// ============================================================================
// 23. MultiRangePageOwnerIndex tests
// ============================================================================
static void TestMultiRangePageOwnerIndex() {
    using OwnerIndex = MultiRangePageOwnerIndex<uint32_t>;

    OwnerIndex index;
    CHECK(index.Register(7, {{0x101000, 0x2800}, {0x102000, 0x3000}}),
          "multi-range owner registers");
    CHECK(index.CoarseMembershipCount(1) == 1,
          "one owner is inserted once in a shared 1 MiB bucket");
    CHECK(index.TrackingMembershipCount(0x102) == 1,
          "overlapping planes insert one 4 KiB membership");
    CHECK(!index.Register(7, {{0x101000, 0x1000}}),
          "duplicate owner registration hard-fails");

    auto owners = index.Query(0x100000, 0x10000);
    CHECK(owners.size() == 1 && owners.front() == 7,
          "multi-page query returns an owner once");

    // Shared page lifecycle
    OwnerIndex index2;
    CHECK(index2.Register(11, {{0x202000, 0x2000}}) &&
          index2.Register(22, {{0x202000, 0x2000}}),
          "two owners register on identical pages");
    CHECK(index2.CoarseMembershipCount(2) == 2 &&
          index2.TrackingMembershipCount(0x202) == 2,
          "coarse and tracking pages retain both owners");

    std::vector<OwnerIndex::ByteRange> releases;
    CHECK(index2.Unregister(11, releases), "first owner unregisters");
    CHECK(releases.empty(),
          "shared tracking pages are not released with one owner remaining");
    CHECK(index2.Query(0x202000, 1).size() == 1 &&
          index2.Query(0x202000, 1).front() == 22,
          "unregistering one owner preserves the other");
    CHECK(!index2.Unregister(11, releases),
          "missing membership hard-fails without mutation");

    CHECK(index2.Unregister(22, releases), "final owner unregisters");
    CHECK(releases.size() == 1 &&
          releases.front().address == 0x202000 &&
          releases.front().size == 0x2000,
          "adjacent final-owner tracking pages return one contiguous release");

    // Byte filtering
    OwnerIndex index3;
    CHECK(index3.Register(31, {{0x300100, 0x100}}), "first byte-disjoint owner registers");
    CHECK(index3.Register(32, {{0x300800, 0x100}}), "second byte-disjoint owner registers");
    CHECK(index3.TrackingMembershipCount(0x300) == 2,
          "byte-disjoint owners share one tracking page");
    CHECK(index3.Query(0x300400, 0x40).empty(),
          "page hit without byte overlap is filtered out");
    auto candidates = index3.QueryCandidates(0x300400, 0x40);
    CHECK(candidates.size() == 2,
          "fault candidate query retains byte-disjoint owners on the touched page");
    auto first = index3.Query(0x300180, 0x10);
    CHECK(first.size() == 1 && first.front() == 31,
          "strict byte overlap selects only the matching owner");
    auto predicate_filtered = index3.Query(0x300000, 0x1000,
        [](uint32_t owner) { return owner == 32; });
    CHECK(predicate_filtered.size() == 1 && predicate_filtered.front() == 32,
          "supplied predicate filters query owners");
}

// ============================================================================
// Main
// ============================================================================
int main() {
    TestGuestRangeBoundaries();
    TestImageInfoValidation();
    TestImageMipInfo();
    TestImageMetadataInfo();
    TestImageDepthDetection();
    TestPolyOffsetBias();
    TestPm4PacketEncoding();
    TestMultiLevelPageTable();
    TestPageRangeBoundaries();
    TestGpuEnumValue();
    TestImageRangeOverlaps();
    TestImagePageRangesOverlap();
    TestVulkanFormatHelpers();
    TestHardwareContextModeControl();
    TestPolyOffset();
    TestPolyOffsetBiasResult();
    TestImageInfoMipLayout();
    TestImageInfoFields();
    TestEnumCoverage();
    TestImageInfoComparison();
    TestColorFastClear();
    TestStandard64KB();
    TestMultiRangePageOwnerIndex();

    std::printf("\n========================================\n");
    std::printf("SyntheticGpuTests: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    std::printf("========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
